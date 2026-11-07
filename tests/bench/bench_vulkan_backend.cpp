#include "bench_timer.hpp"
#include "malloc_blocks.hpp"
#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/executor/vulkan_backend_probe.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using av::BenchTimer;
using av::VulkanBackend;
using av::VulkanBackendProbe;
using HandleKind = av::vkproto::HandleKind;

namespace {

constexpr std::uint32_t most_fences = 256u;

struct FakeDriver {
    bool          signalled[most_fences] = {};
    std::uint32_t made = 0u;
    std::uint32_t draws = 0u;
};

FakeDriver driver;

template <typename H>
H as_handle(std::uint64_t raw)
{
    H handle = VK_NULL_HANDLE;
    std::memcpy(&handle, &raw, sizeof handle);
    return handle;
}

std::uint32_t index_of(VkFence fence)
{
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fence)) - 1u;
}

constexpr std::uint64_t instance = 0x100u;
constexpr std::uint64_t physical = 0x200u;
constexpr std::uint64_t device = 0x300u;
constexpr std::uint64_t queue = 0x400u;
constexpr std::uint64_t command_pool = 0x500u;
constexpr std::uint64_t first_buffer = 0x1000u;
constexpr std::uint64_t cb = 0x600u;

}

extern "C" {

VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice, const VkFenceCreateInfo *, const VkAllocationCallbacks *, VkFence *fence)
{
    if (driver.made == most_fences) return VK_ERROR_OUT_OF_HOST_MEMORY;
    driver.signalled[driver.made] = false;
    *fence = reinterpret_cast<VkFence>(static_cast<std::uintptr_t>(driver.made++) + 1u);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks *) {}

VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t count, const VkFence *fences)
{
    for (uint32_t at = 0u; at < count; at++) driver.signalled[index_of(fences[at])] = false;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice, VkFence fence)
{
    return driver.signalled[index_of(fence)] ? VK_SUCCESS : VK_NOT_READY;
}

VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t, const VkFence *fences, VkBool32, uint64_t)
{
    return driver.signalled[index_of(fences[0])] ? VK_SUCCESS : VK_TIMEOUT;
}

VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo *, VkFence fence)
{
    if (fence != VK_NULL_HANDLE) driver.signalled[index_of(fence)] = true;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice)
{
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t *count,
                                                                    VkQueueFamilyProperties *families)
{
    if (families == nullptr) {
        *count = 1u;
        return;
    }
    families[0] = VkQueueFamilyProperties{};
    families[0].queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT;
    families[0].queueCount = 1u;
    *count = 1u;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo *, const VkAllocationCallbacks *,
                                                   VkCommandPool *pool)
{
    *pool = reinterpret_cast<VkCommandPool>(std::uintptr_t{0x500u});
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice, const VkAllocationCallbacks *) {}

VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance, const VkAllocationCallbacks *) {}

VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks *) {}

VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer *) {}

VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks *) {}

VKAPI_ATTR void VKAPI_CALL vkCmdBindVertexBuffers(VkCommandBuffer, uint32_t, uint32_t, const VkBuffer *, const VkDeviceSize *)
{
    driver.draws++;
}

}

namespace {

struct Rig {
    Rig() : blocks_(), sink_(), lines_(sink_, "vulkan: "), backend_(blocks_, lines_), probe_(backend_)
    {
        driver = FakeDriver{};
        backend_.made(HandleKind::instance, instance);
        backend_.made_through(instance);
        backend_.made(HandleKind::physical_device, physical);
        backend_.made_through(physical);
        backend_.made(HandleKind::device, device);
        backend_.made_through(device);
        backend_.made(HandleKind::queue, queue);
        VkCommandPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        VkCommandPool           made_pool = VK_NULL_HANDLE;
        if (backend_.on_vkCreateCommandPool(as_handle<VkDevice>(device), &pool_info, nullptr, &made_pool) != VK_SUCCESS)
            std::exit(1);
        backend_.made(HandleKind::command_pool, command_pool);
        backend_.made_through(command_pool);
        backend_.made(HandleKind::command_buffer, cb);
        backend_.made_through(device);
        for (std::uint64_t at = 0u; at < 64u; at++) backend_.made(HandleKind::buffer, first_buffer + at);
        backend_.made_through(0u);
    }
    ~Rig() { backend_.shut_down(); }

    av::MallocBlocks     blocks_;
    av::RecordingLog     sink_;
    av::DriverLog        lines_;
    VulkanBackend        backend_;
    VulkanBackendProbe   probe_;
};

void commands(const BenchTimer &timer)
{
    Rig                  rig;
    alignas(16) std::uint8_t arena_block[4096];
    std::uint8_t         request[256];
    std::uint8_t         answer[64];
    const std::uint64_t  count = 200000u;
    const double each = timer.nanoseconds_per_operation(count, [&] {
        for (std::uint64_t at = 0u; at < count; at++) {
            av::WireWriter writer(request, sizeof request);
            writer.write_u64(cb);
            writer.write_u32(0u);
            writer.write_u32(3u);
            for (std::uint64_t index = 0u; index < 3u; index++) writer.write_u64(first_buffer + (at + index * 7u) % 64u);
            for (std::uint64_t index = 0u; index < 3u; index++) writer.write_u64(0u);
            av::WireReader payload(request, writer.offset());
            av::WireWriter reply(answer, sizeof answer);
            av::WireArena  arena(arena_block, sizeof arena_block);
            if (!rig.backend_.run(av::vkproto::kCmd_vkCmdBindVertexBuffers, payload, reply, arena)) {
                std::fprintf(stderr, "run failed\n");
                std::exit(1);
            }
        }
    });
    if (driver.draws == 0u) {
        std::fprintf(stderr, "no command reached the driver\n");
        std::exit(1);
    }
    timer.report("vkCmdBindVertexBuffers with 3 buffers (backend run)", count, each);
}

void submits(const BenchTimer &timer)
{
    Rig                  rig;
    VkCommandBuffer      buffer = as_handle<VkCommandBuffer>(cb);
    VkSubmitInfo         submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1u;
    submit.pCommandBuffers = &buffer;
    rig.probe_.begin_frame(true);
    std::uint64_t handle = 0u;
    if (!rig.backend_.handle_for(HandleKind::command_buffer, cb, handle)) std::exit(1);
    for (std::uint64_t at = 0u; at < 24u; at++)
        if (!rig.backend_.handle_for(HandleKind::buffer, first_buffer + at, handle)) std::exit(1);
    rig.probe_.begin_frame(false);
    const std::uint64_t count = 100000u;
    const double each = timer.nanoseconds_per_operation(count, [&] {
        for (std::uint64_t at = 0u; at < count; at++) {
            if (rig.backend_.on_vkQueueSubmit(as_handle<VkQueue>(queue), 1u, &submit, VK_NULL_HANDLE) != VK_SUCCESS) {
                std::fprintf(stderr, "submit failed\n");
                std::exit(1);
            }
            rig.backend_.end_batch();
        }
    });
    timer.report("vkQueueSubmit (backend, driver calls faked, 1 cb of 24)", count, each);
}

}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    const BenchTimer timer(5u);
    commands(timer);
    submits(timer);
    return 0;
}
