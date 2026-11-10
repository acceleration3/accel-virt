#include "core/support/object_storage.hpp"
#include "device/abi/av_executor.h"
#include "executor/sharing/session_shares.hpp"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/executor_log.hpp"
#include "executor/import_table.hpp"
#include "proto/submission/blob_role.hpp"
#include "executor/source_memory.hpp"
#include "executor/d3d/query_result_publisher.hpp"
#include "proto/submission/published_slot_layout.hpp"
#include "harness/executor/local_import.hpp"
#include "core/log/driver_log.hpp"
#include "executor/sharing/host_shares.hpp"
#include "harness/icd/recording_log.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

#include <vulkan/vulkan.h>

namespace av {
struct D3dBackendProbe {
    static D3dObjectTable &objects(D3dBackend &backend) { return backend.objects_; }
    static GuestBytes     &guest(D3dBackend &backend) { return backend.guest_; }
};

}
using av::D3dBackend;
using av::BlobRole;
using av::LocalImport;
using namespace av::d3dproto;

namespace {

constexpr std::uint32_t nobody = 77u;
constexpr std::uint64_t page_bytes = 4096u;

av::RecordingLog              backend_sink;
av::ExecutorLog               backend_log(backend_sink);
av::ObjectStorage<D3dBackend> backend_storage;
alignas(4096) std::uint8_t guest[2u * page_bytes];
alignas(4096) std::uint8_t store_bytes[page_bytes];

class FreshBackend {
public:
    FreshBackend() { CHECK_STATUS(backend_storage.construct(backend_log), av::Status::ok); }
    ~FreshBackend()
    {
        backend_storage.object().shut_down();
        backend_storage.destroy();
    }
    FreshBackend(const FreshBackend &) = delete;
    FreshBackend &operator=(const FreshBackend &) = delete;

    D3dBackend &backend() { return backend_storage.object(); }
};

std::uint64_t address_of(const av::SourceMemory &memory)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&memory));
}

std::uint8_t *guest_at(D3dBackend &backend, std::uint64_t offset, std::uint64_t bytes)
{
    return av::D3dBackendProbe::guest(backend).at(offset, bytes);
}

bool adopt_arena(D3dBackend &backend, av::LocalImport &region, std::uint64_t base = 0u)
{
    std::uint64_t handle = 0u;
    return backend.adopt(region.memory(), BlobRole(BlobRole::Kind::arena_segment, base), handle) && handle != 0u;
}

}

TEST(d3d_backend_adopt, an_arena_segment_binds_its_bytes_at_its_base_and_nothing_else)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  CHECK(guest_at(backend, 0u, 1u) == nullptr);
  LocalImport region(guest, sizeof guest);
  CHECK(adopt_arena(backend, region));
  CHECK(guest_at(backend, 0u, 16u) == guest);
  CHECK(guest_at(backend, page_bytes, page_bytes) == guest + page_bytes);
  CHECK(guest_at(backend, page_bytes, page_bytes + 1u) == nullptr);
  CHECK(guest_at(backend, 8u, 8u) == guest + 8u);
  LocalImport second(store_bytes, sizeof store_bytes, 2u);
  CHECK(adopt_arena(backend, second, av::GuestBytes::slot_bytes));
  CHECK(guest_at(backend, av::GuestBytes::slot_bytes, 16u) == store_bytes);
  CHECK(guest_at(backend, 0u, 16u) == guest);
  backend.unbind_arena(second.memory());
  CHECK(guest_at(backend, av::GuestBytes::slot_bytes, 16u) == nullptr);
  backend.unbind_arena(region.memory());
  CHECK(guest_at(backend, 0u, 16u) == nullptr);
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend_adopt, a_plain_import_is_an_object_memory_named_by_its_address_and_never_binds_bytes)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  LocalImport region(guest, sizeof guest, 1u);
  LocalImport object(store_bytes, sizeof store_bytes, 2u);
  CHECK(adopt_arena(backend, region));
  std::uint64_t handle = 0u;
  CHECK(backend.adopt(object.memory(), BlobRole(), handle));
  CHECK_EQ(handle, address_of(object.memory()));
  CHECK(guest_at(backend, 0u, sizeof guest) == guest);
  CHECK(guest_at(backend, sizeof guest, 1u) == nullptr);
  backend.abandon(object.memory(), handle);
  CHECK(guest_at(backend, 0u, 16u) == guest);
  backend.unbind_arena(object.memory());
  CHECK(guest_at(backend, 0u, 16u) == guest);
  backend.unbind_arena(region.memory());
  CHECK(guest_at(backend, 0u, 16u) == nullptr);
  LocalImport next(guest, page_bytes, 3u);
  CHECK(adopt_arena(backend, next));
  CHECK(guest_at(backend, page_bytes - 1u, 1u) == guest + page_bytes - 1u);
  CHECK(guest_at(backend, page_bytes, 1u) == nullptr);
  backend.retire_arena(next.memory());
  backend.unbind_arena(next.memory());
  CHECK(guest_at(backend, 0u, 1u) == nullptr);
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend_unbind_arena, a_memory_that_was_never_an_arena_segment_unbinds_nothing)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  LocalImport region(guest, sizeof guest);
  CHECK(adopt_arena(backend, region));
  LocalImport stranger(store_bytes, sizeof store_bytes, 9u);
  backend.unbind_arena(stranger.memory());
  backend.retire_arena(stranger.memory());
  CHECK(guest_at(backend, 0u, 16u) == guest);
  backend.unbind_arena(region.memory());
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend_on_BindQueryResults, binds_slots_only_inside_the_arena)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  CHECK(!backend.on_BindQueryResults(0u, 1u));
  LocalImport region(guest, sizeof guest);
  CHECK(adopt_arena(backend, region));
  const std::uint32_t fit = (sizeof guest - av::PublishedSlotLayout::error_word_bytes) / av::PublishedSlotLayout::slot_bytes;
  CHECK(backend.on_BindQueryResults(0u, fit));
  CHECK(!backend.on_BindQueryResults(0u, fit + 1u));
  CHECK(!backend.on_BindQueryResults(av::PublishedSlotLayout::slot_bytes, fit));
  CHECK(!backend.on_BindQueryResults(1u, 1u));
  CHECK(!backend.on_BindQueryResults(0u, 0u));
  CHECK(!backend.on_BindQueryResults(0u, 4097u));
  backend.unbind_arena(region.memory()); }

TEST(d3d_backend_on_BindBufferSlice, a_session_with_no_device_has_no_slices)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  std::uint32_t supported = 1u;
  CHECK(backend.on_QueryBufferSlices11(supported));
  CHECK_EQ(supported, 0u);
  LocalImport region(guest, sizeof guest, 1u);
  LocalImport store(store_bytes, sizeof store_bytes, 2u);
  std::uint64_t stored = 0u;
  CHECK(adopt_arena(backend, region));
  CHECK(backend.adopt(store.memory(), BlobRole(), stored));
  CHECK(!backend.on_BindBufferSlice(1u, stored, 0u, 256u));
  backend.abandon(store.memory(), stored);
  backend.unbind_arena(region.memory()); }

TEST(d3d_backend, makes_nothing_before_the_guest_made_a_device)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  ResourceDesc resource = {};
  resource.width = 256u;
  const std::uint8_t code[8] = {};
  const InputElement element = {};
  std::uint32_t found = 9u;
  ResourceDesc opened = {};
  opened.width = 5u;
  std::uint32_t hresult = 9u;
  CHECK(!backend.on_CreateResource(1u, resource, 0u, 0u, 0u));
  CHECK(!backend.on_OpenResource(2u, 0x51u, found, opened));
  CHECK_EQ(found, 0u);
  CHECK(opened.width == 0u);
  CHECK(!backend.on_CreateShader(3u, 0u, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, nullptr, 0u, code,
                                 sizeof code));
  CHECK(!backend.on_CreateInputLayout(4u, 1u, &element));
  CHECK(!backend.on_CreateBlendState(5u, BlendDesc{}));
  CHECK(!backend.on_CreateDepthStencilState(6u, DepthStencilDesc{}));
  CHECK(!backend.on_CreateRasterizerState(7u, RasterizerDesc{}));
  CHECK(!backend.on_CreateSampler(8u, SamplerDesc{}));
  CHECK(!backend.on_CreateQuery(9u, 0u, 0u));
  CHECK(!backend.on_Flush11());
  CHECK(!backend.on_ClearState11());
  CHECK(!backend.on_Finish11(hresult));
  const std::uint32_t pair[2] = {1u, 2u};
  CHECK(!backend.on_RotateResources11(2u, pair));
  CHECK(!backend.on_DestroyDevice11(nobody));
  for (std::uint32_t id = 1u; id <= 9u; id++) CHECK(!backend.on_DestroyObject(id));
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend, a_view_of_a_resource_it_never_made_is_refused)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  ViewDesc desc = {};
  desc.resource = nobody;
  CHECK(!backend.on_CreateShaderResourceView(1u, desc));
  CHECK(!backend.on_CreateRenderTargetView(2u, desc));
  CHECK(!backend.on_CreateDepthStencilView(3u, desc));
  CHECK(!backend.on_CreateUnorderedAccessView(4u, desc)); }

TEST(d3d_backend, nothing_is_bound_drawn_cleared_or_copied_with_no_device_selected)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  const std::uint32_t ids[1] = {nobody};
  const ConstantBufferBinding constants = {};
  const UavBinding uav = {};
  const VertexBufferBinding vertices = {};
  const StreamOutTarget stream = {};
  const Viewport viewport = {};
  const Rect rect = {};
  const Float4 color = {};
  const Uint4 values = {};
  const Box box = {};
  const std::uint8_t data[16] = {};
  std::uint8_t   out[16] = {};
  std::uint32_t  hresult = 0u, row_pitch = 0u, depth_pitch = 0u, ready = 9u;
  std::uint64_t  size = 9u;
  CHECK(!backend.on_SetShader(0u, nobody));
  CHECK(!backend.on_SetConstantBuffers(0u, 0u, 1u, &constants));
  CHECK(!backend.on_SetShaderResources(0u, 0u, 1u, ids));
  CHECK(!backend.on_SetSamplers(0u, 0u, 1u, ids));
  CHECK(!backend.on_SetComputeUnorderedAccessViews(0u, 1u, &uav));
  CHECK(!backend.on_SetRenderTargets(1u, ids, 0u, 0u, 0u, nullptr));
  CHECK(!backend.on_SetInputLayout(0u));
  CHECK(!backend.on_SetVertexBuffers(0u, 1u, &vertices));
  CHECK(!backend.on_SetIndexBuffer(0u, 42u, 0u));
  CHECK(!backend.on_SetTopology(4u));
  CHECK(!backend.on_SetViewports(1u, &viewport));
  CHECK(!backend.on_SetScissorRects(1u, &rect));
  CHECK(!backend.on_SetBlendState(0u, color, 0xFFFFFFFFu));
  CHECK(!backend.on_SetDepthStencilState(0u, 0u));
  CHECK(!backend.on_SetRasterizerState(0u));
  CHECK(!backend.on_SetStreamOutTargets(1u, &stream));
  CHECK(!backend.on_SetPredication(0u, 0u));
  CHECK(!backend.on_SetResourceMinLod(nobody, 1.0f));
  CHECK(!backend.on_Draw(3u, 0u));
  CHECK(!backend.on_DrawIndexed(3u, 0u, 0));
  CHECK(!backend.on_DrawInstanced(3u, 1u, 0u, 0u));
  CHECK(!backend.on_DrawIndexedInstanced(3u, 1u, 0u, 0, 0u));
  CHECK(!backend.on_DrawAuto());
  CHECK(!backend.on_DrawInstancedIndirect(nobody, 0u));
  CHECK(!backend.on_DrawIndexedInstancedIndirect(nobody, 0u));
  CHECK(!backend.on_Dispatch(1u, 1u, 1u));
  CHECK(!backend.on_DispatchIndirect(nobody, 0u));
  CHECK(!backend.on_ClearRenderTargetView(nobody, color));
  CHECK(!backend.on_ClearDepthStencilView(nobody, 1u, 1.0f, 0u));
  CHECK(!backend.on_ClearUnorderedAccessViewUint(nobody, values));
  CHECK(!backend.on_ClearUnorderedAccessViewFloat(nobody, color));
  CHECK(!backend.on_ClearView(nobody, color, 1u, &rect));
  CHECK(!backend.on_Discard(nobody, 0u, nullptr));
  CHECK(!backend.on_CopyResource(1u, 2u));
  CHECK(!backend.on_CopySubresourceRegion(1u, 0u, 0u, 0u, 0u, 2u, 0u, 0u, box, 0u));
  CHECK(!backend.on_BltResource(1u, 0u, 0u, 0u, 4u, 4u, 2u, 0u, 0u, 0u, 4u, 4u));
  CHECK(!backend.on_ResolveSubresource(1u, 0u, 2u, 0u, 28u));
  CHECK(!backend.on_GenerateMips(nobody));
  CHECK(!backend.on_CopyStructureCount(1u, 0u, 2u));
  std::uint32_t tiles = 0u, packed_mips = 0u, packed_tiles = 0u;
  const TileRegion tile_region = {};
  const TileRange  tile_range = {};
  const TileCoordinate tile_start = {};
  const TileSize       tile_size = {};
  CHECK(!backend.on_ResourceTiling11(1u, tiles, packed_mips, packed_tiles));
  CHECK(!backend.on_UpdateTileMappings11(1u, 1u, &tile_region, 2u, 1u, &tile_range, 0u));
  CHECK(!backend.on_CopyTileMappings11(1u, tile_start, 2u, tile_start, tile_size, 0u));
  CHECK(!backend.on_CopyTiles11(1u, tile_start, tile_size, 2u, 0u, 0u));
  CHECK(!backend.on_UpdateTiles11(1u, tile_start, tile_size, 0u, data, sizeof data));
  CHECK(!backend.on_ResizeTilePool11(1u, 0u));
  CHECK(!backend.on_TiledResourceBarrier11(1u, 2u));
  CHECK(!backend.on_UpdateSubresource(nobody, 0u, 0u, box, 16u, 16u, 0u, data, sizeof data));
  CHECK(!backend.on_ReadSubresource(nobody, 0u, hresult, row_pitch, depth_pitch, out, sizeof out, size));
  CHECK(!backend.on_PresentPoint(nobody, 0u, 1u));
  CHECK(!backend.on_BeginQuery(nobody));
  CHECK(!backend.on_EndQuery(nobody));
  CHECK(!backend.on_GetQueryData(nobody, 0u, ready, out, sizeof out, size));
  CHECK_EQ(ready, 0u);
  CHECK(size == 0u);
  CHECK(backend.invariants_hold()); }

TEST(d3d_backend, a_backend_with_no_work_has_nothing_outstanding)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  std::uint64_t marked = 3u, reached = 3u;
  backend.gpu_progress(marked, reached);
  CHECK(marked == 0u);
  CHECK(reached == 0u);
  CHECK(!backend.poll());
  backend.end_batch();
  CHECK(!backend.lost()); }

namespace {

bool first_gpu(std::uint8_t *uuid)
{
    VkApplicationInfo application = {};
    application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    application.apiVersion = (1u << 22u) | (1u << 12u);
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

struct Selected {
    std::uint32_t ran;
    std::uint32_t created_hresult;
    std::uint32_t selected_known;
    std::uint32_t drew_on_known;
    std::uint32_t selected_unknown;
    std::uint32_t drew_on_unknown;
    std::uint32_t selected_again;
    std::uint32_t drew_again;
};

Selected select_devices()
{
    Selected seen = {};
    if (std::getenv("AV_D3D_PATH") == nullptr) setenv("AV_D3D_PATH", AV_FIXTURE_DIR "/../../build-d3d/lib", 1);
    av::D3dRuntime      runtime;
    av::HostHeap        heap;
    av::RecordingLog    log;
    av::DriverLog       lines(log, "shares: ");
    av::HostShares      shares(heap, lines);
    av::ImportTable     imports(heap);
    av::D3d11HostDevice device11(lines);
    std::uint8_t        uuid[VK_UUID_SIZE] = {};
    if (!runtime.open() || !first_gpu(uuid) || !device11.open(runtime, uuid, shares)) return seen;
    FreshBackend fresh;
    D3dBackend  &backend = fresh.backend();
    av::SessionShares unshared;
    if (!backend.initialize(unshared, imports, &device11, nullptr)) return seen;
    seen.ran = 1u;
    device11.lock(nullptr);
    std::uint32_t hresult = 1u, level = 0u;
    static_cast<void>(backend.on_CreateDevice11(1u, 0u, 0u, hresult, level));
    seen.created_hresult = hresult;
    seen.selected_known = backend.on_SelectDevice11(1u) ? 1u : 0u;
    seen.drew_on_known = backend.on_Draw(3u, 0u) ? 1u : 0u;
    seen.selected_unknown = backend.on_SelectDevice11(99u) ? 1u : 0u;
    seen.drew_on_unknown = backend.on_Draw(3u, 0u) ? 1u : 0u;
    seen.selected_again = backend.on_SelectDevice11(1u) ? 1u : 0u;
    seen.drew_again = backend.on_Draw(3u, 0u) ? 1u : 0u;
    device11.unlock();
    backend.shut_down();
    return seen;
}

}

TEST(d3d_backend_on_SelectDevice11, a_known_device_is_drawn_on_and_an_unknown_one_drops_what_follows)
{ int channel[2] = {-1, -1};
  CHECK_EQ(pipe(channel), 0);
  const pid_t child = fork();
  CHECK(child >= 0);
  if (child == 0) {
      const Selected seen = select_devices();
      static_cast<void>(write(channel[1], &seen, sizeof seen));
      _exit(0);
  }
  close(channel[1]);
  Selected seen = {};
  const ssize_t got = read(channel[0], &seen, sizeof seen);
  close(channel[0]);
  int status = 0;
  CHECK_EQ(waitpid(child, &status, 0), child);
  CHECK_EQ(got, static_cast<ssize_t>(sizeof seen));
  if (seen.ran == 0u) return;
  CHECK_EQ(seen.created_hresult, 0u);
  CHECK_EQ(seen.selected_known, 1u);
  CHECK_EQ(seen.drew_on_known, 1u);
  CHECK_EQ(seen.selected_unknown, 1u);
  CHECK_EQ(seen.drew_on_unknown, 0u);
  CHECK_EQ(seen.selected_again, 1u);
  CHECK_EQ(seen.drew_again, 1u); }

namespace {

class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override { return --references; }

    ULONG references = 1u;
};

}

TEST(d3d_backend_on_DestroyObject, removes_only_direct3d_11_objects)
{ FreshBackend fresh;
  D3dBackend &backend = fresh.backend();
  av::D3dObjectTable &objects = av::D3dBackendProbe::objects(backend);
  Counted eleven, twelve, nine;
  CHECK_STATUS(objects.insert(51u, av::D3dObjectKind::resource, &eleven), av::Status::ok);
  CHECK_STATUS(objects.insert(52u, av::D3dObjectKind::heap12, &twelve), av::Status::ok);
  CHECK_STATUS(objects.insert(53u, av::D3dObjectKind::surface9, &nine), av::Status::ok);
  CHECK(!backend.on_DestroyObject(52u));
  CHECK(!backend.on_DestroyObject(53u));
  CHECK_EQ(twelve.references, 1u);
  CHECK(backend.on_DestroyObject(51u));
  CHECK_EQ(eleven.references, 0u);
  objects.remove(52u);
  objects.remove(53u); }
