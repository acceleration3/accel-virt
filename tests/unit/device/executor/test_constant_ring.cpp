/* Small constant buffers kept in one ring buffer, on the host's DXVK: a
 * whole-buffer write of a dynamic constant buffer goes into the ring, a bind
 * of it is a window of the ring in whole 16-constant blocks, a buffer written
 * while bound is rebound where it moved, and a buffer let go gets its
 * contents back in its own storage. DXVK's threads leak from modules it
 * unloads before exit, so the GPU's part runs in a child that leaves with
 * _exit and hands back what it saw; a host without DXVK on a GPU returns. */
#include "device/executor/constant_ring.hpp"
#include "device/shim/d3d_headers.hpp"
#include "device/shim/d3d_runtime.hpp"
#include "harness/framework/test_registry.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

using av::ConstantRing;

namespace {

bool first_gpu(std::uint8_t *uuid)
{
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = (1u << 22u) | (1u << 12u);   /* 1.1 */
    VkInstanceCreateInfo made = {};
    made.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    made.pApplicationInfo = &application;
    VkInstance instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&made, nullptr, &instance) != VK_SUCCESS) return false;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    std::uint32_t    count = 1u;
    const bool       found = vkEnumeratePhysicalDevices(instance, &count, &physical) >= 0 && count != 0u;
    if (found) {
        VkPhysicalDeviceIDProperties ids = {};
        ids.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES;
        VkPhysicalDeviceProperties2 properties = {};
        properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties.pNext = &ids;
        vkGetPhysicalDeviceProperties2(physical, &properties);
        std::memcpy(uuid, ids.deviceUUID, VK_UUID_SIZE);
    }
    vkDestroyInstance(instance, nullptr);
    return found;
}

/* What the child saw. */
struct Seen {
    std::uint32_t ran;
    std::uint32_t kept_dynamic;
    std::uint32_t kept_default;
    std::uint32_t kept_wrong_size;
    std::uint32_t kept_too_large;
    std::uint32_t bound_to_ring;
    std::uint32_t first;
    std::uint32_t count;
    std::uint32_t second_first;
    std::uint32_t second_count;
    std::uint32_t unknown_left_alone;
    std::uint32_t rebound_first;
    std::uint32_t rebound_to_ring;
    std::uint32_t released_back_to_own;
    std::uint32_t released_contents;
    std::uint32_t forgotten_left_alone;
    std::uint32_t closed_left_alone;
};

ID3D11Buffer *constant_buffer(ID3D11Device5 *device, std::uint32_t bytes, D3D11_USAGE usage)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = bytes;
    desc.Usage = usage;
    desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0u;
    ID3D11Buffer *made = nullptr;
    return SUCCEEDED(device->CreateBuffer(&desc, nullptr, &made)) ? made : nullptr;
}

/* The first byte of `buffer`, copied through a staging buffer. */
std::uint32_t first_byte(ID3D11Device5 *device, ID3D11DeviceContext4 *context, ID3D11Buffer *buffer)
{
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth = 256u;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Buffer *staging = nullptr;
    if (FAILED(device->CreateBuffer(&desc, nullptr, &staging))) return 0xFFFFu;
    context->CopyResource(staging, buffer);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    std::uint32_t byte = 0xFFFFu;
    if (SUCCEEDED(context->Map(staging, 0u, D3D11_MAP_READ, 0u, &mapped))) {
        byte = static_cast<const std::uint8_t *>(mapped.pData)[0];
        context->Unmap(staging, 0u);
    }
    staging->Release();
    return byte;
}

void run_ring(Seen &seen)
{
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::D3dRuntime runtime;
    std::uint8_t   uuid[VK_UUID_SIZE] = {};
    if (!runtime.open() || !first_gpu(uuid)) return;
    IDXGIAdapter1 *const adapter = runtime.adapter_for(uuid);
    if (adapter == nullptr) return;
    ID3D11Device        *base = nullptr;
    ID3D11DeviceContext *immediate = nullptr;
    std::uint32_t        level = 0u;
    const bool           made = runtime.create_device11(adapter, base, immediate, level) == S_OK;
    adapter->Release();
    if (!made) return;
    ID3D11Device5        *device = nullptr;
    ID3D11DeviceContext4 *context = nullptr;
    if (FAILED(base->QueryInterface(__uuidof(ID3D11Device5), reinterpret_cast<void **>(&device))) ||
        FAILED(immediate->QueryInterface(__uuidof(ID3D11DeviceContext4), reinterpret_cast<void **>(&context))))
        return;
    ID3D11Buffer *const small = constant_buffer(device, 256u, D3D11_USAGE_DYNAMIC);
    ID3D11Buffer *const larger = constant_buffer(device, 512u, D3D11_USAGE_DYNAMIC);
    ID3D11Buffer *const fixed = constant_buffer(device, 256u, D3D11_USAGE_DEFAULT);
    if (small == nullptr || larger == nullptr || fixed == nullptr) return;
    static ConstantRing ring;
    std::uint8_t data[512];
    std::memset(data, 0x5A, sizeof data);
    seen.ran = 1u;
    constexpr std::uint32_t device_id = 1u;
    seen.kept_dynamic = ring.update(device, context, device_id, 10u, small, data, 256u) ? 1u : 0u;
    seen.kept_default = ring.update(device, context, device_id, 12u, fixed, data, 256u) ? 1u : 0u;
    seen.kept_wrong_size = ring.update(device, context, device_id, 13u, larger, data, 256u) ? 1u : 0u;
    seen.kept_too_large = ring.update(device, context, device_id, 14u, larger, data, 0u) ? 1u : 0u;

    std::uint32_t ids[2] = {10u, 77u};
    ID3D11Buffer *buffers[2] = {small, larger};
    std::uint32_t first[2] = {0u, 0u};
    std::uint32_t counts[2] = {0u, 0u};
    ring.bind(device_id, 0u, 0u, 2u, ids, buffers, first, counts);
    seen.bound_to_ring = buffers[0] != small ? 1u : 0u;
    seen.first = first[0];
    seen.count = counts[0];
    seen.unknown_left_alone = buffers[1] == larger && first[1] == 0u ? 1u : 0u;
    context->VSSetConstantBuffers1(0u, 1u, buffers, first, counts);

    /* A second buffer goes after the first, in whole 256-byte windows. */
    if (!ring.update(device, context, device_id, 11u, larger, data, 512u)) return;
    std::uint32_t second_ids[1] = {11u};
    ID3D11Buffer *second_buffers[1] = {larger};
    std::uint32_t second_first[1] = {0u};
    std::uint32_t second_counts[1] = {0u};
    ring.bind(device_id, 4u, 0u, 1u, second_ids, second_buffers, second_first, second_counts);
    seen.second_first = second_first[0];
    seen.second_count = second_counts[0];

    /* Writing the bound buffer again moves it, and its slot follows. */
    data[0] = 0x33u;
    if (!ring.update(device, context, device_id, 10u, small, data, 256u)) return;
    ID3D11Buffer *now = nullptr;
    UINT          now_first = 0u, now_count = 0u;
    context->VSGetConstantBuffers1(0u, 1u, &now, &now_first, &now_count);
    seen.rebound_first = now_first;
    seen.rebound_to_ring = now == buffers[0] ? 1u : 0u;
    if (now != nullptr) now->Release();

    /* Let go: the buffer's own storage holds what the guest wrote, and its slot
     * is the buffer again. */
    ring.release(context, device_id, 10u, small);
    context->VSGetConstantBuffers1(0u, 1u, &now, &now_first, &now_count);
    seen.released_back_to_own = now == small ? 1u : 0u;
    if (now != nullptr) now->Release();
    seen.released_contents = first_byte(device, context, small);

    ring.forget(11u);
    second_buffers[0] = larger;
    ring.bind(device_id, 4u, 0u, 1u, second_ids, second_buffers, second_first, second_counts);
    seen.forgotten_left_alone = second_buffers[0] == larger ? 1u : 0u;

    ring.selected(context, device_id);
    ring.cleared(device_id);
    ring.forget_device(device_id);
    ring.close();
    std::uint32_t again[1] = {10u};
    ID3D11Buffer *again_buffers[1] = {small};
    std::uint32_t again_first[1] = {0u};
    std::uint32_t again_counts[1] = {0u};
    ring.bind(device_id, 0u, 0u, 1u, again, again_buffers, again_first, again_counts);
    seen.closed_left_alone = again_buffers[0] == small ? 1u : 0u;
}

}  // namespace

TEST(constant_ring, keeps_dynamic_constant_buffers_in_one_ring_and_gives_them_back)
{ int channel[2] = {-1, -1};
  CHECK_EQ(pipe(channel), 0);
  const pid_t child = fork();
  CHECK(child >= 0);
  if (child == 0) {
      Seen seen = {};
      run_ring(seen);
      static_cast<void>(write(channel[1], &seen, sizeof seen));
      _exit(0);
  }
  close(channel[1]);
  Seen seen = {};
  const ssize_t got = read(channel[0], &seen, sizeof seen);
  close(channel[0]);
  int status = 0;
  CHECK_EQ(waitpid(child, &status, 0), child);
  CHECK_EQ(got, static_cast<ssize_t>(sizeof seen));
  if (seen.ran == 0u) return;   /* no DXVK on a GPU here: nothing to judge */
  CHECK_EQ(seen.kept_dynamic, 1u);
  CHECK_EQ(seen.kept_default, 0u);      /* only a dynamic buffer is a ring's */
  CHECK_EQ(seen.kept_wrong_size, 0u);   /* only a whole-buffer write */
  CHECK_EQ(seen.kept_too_large, 0u);
  CHECK_EQ(seen.bound_to_ring, 1u);
  CHECK_EQ(seen.first, 0u);
  CHECK_EQ(seen.count, 16u);            /* 256 bytes of 16-byte constants */
  CHECK_EQ(seen.unknown_left_alone, 1u);
  CHECK_EQ(seen.second_first, 16u);     /* after the first window */
  CHECK_EQ(seen.second_count, 32u);
  CHECK_EQ(seen.rebound_to_ring, 1u);
  CHECK_EQ(seen.rebound_first, 48u);    /* after both */
  CHECK_EQ(seen.released_back_to_own, 1u);
  CHECK_EQ(seen.released_contents, 0x33u);
  CHECK_EQ(seen.forgotten_left_alone, 1u);
  CHECK_EQ(seen.closed_left_alone, 1u); }
