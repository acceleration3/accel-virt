#ifndef AV_VULKAN_WIRE_RIG_HPP
#define AV_VULKAN_WIRE_RIG_HPP

#include "core/log/driver_log.hpp"
#include "executor/vulkan/vulkan_backend.hpp"
#include "harness/icd/recording_log.hpp"
#include "proto/generated/vulkan/vk_handle_translation.h"
#include "proto/generated/vulkan/vk_protocol_ids.h"
#include "proto/generated/vulkan/vk_serialize.h"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace av {

class VulkanWireRig {
public:
    static constexpr std::uint64_t instance = 0x1100u;
    static constexpr std::uint64_t physical = 0x1200u;
    static constexpr std::uint64_t device = 0x1300u;
    static constexpr std::uint64_t queue = 0x1400u;
    static constexpr std::uint64_t other_queue = 0x1500u;
    static constexpr std::size_t   frame_bytes = 4096u;

    VulkanWireRig() : sink_(), lines_(sink_, "vulkan: "), backend_(lines_), made_(0x7000u), block_{}, frame_{}, reply_{}
    {
        known(0u, vkproto::HandleKind::instance, instance);
        known(instance, vkproto::HandleKind::physical_device, physical);
        known(physical, vkproto::HandleKind::device, device);
        known(device, vkproto::HandleKind::queue, queue);
        known(device, vkproto::HandleKind::queue, other_queue);
    }

    VulkanBackend &backend() { return backend_; }
    RecordingLog  &sink() { return sink_; }

    void known(std::uint64_t parent, vkproto::HandleKind kind, std::uint64_t handle)
    {
        backend_.made_through(parent);
        backend_.made(kind, handle);
        backend_.made_through(0u);
    }

    std::uint64_t make(std::uint64_t parent, vkproto::HandleKind kind)
    {
        made_ += 0x100u;
        known(parent, kind, made_);
        return made_;
    }

    WireWriter frame() { return WireWriter(frame_, sizeof frame_); }

    bool send(std::uint32_t command, const WireWriter &frame)
    {
        WireArena  arena(block_, sizeof block_);
        WireReader payload(frame_, frame.offset());
        WireWriter answer(reply_, sizeof reply_);
        const bool ran = backend_.run(command, payload, answer, arena);
        answered_ = answer.offset();
        return ran;
    }

    std::int32_t result() const
    {
        std::int32_t value = 0;
        if (answered_ >= sizeof value) std::memcpy(&value, reply_, sizeof value);
        return value;
    }

    std::uint64_t answered_handle() const
    {
        std::uint64_t handle = 0u;
        if (answered_ >= sizeof(std::int32_t) + sizeof handle) std::memcpy(&handle, reply_ + sizeof(std::int32_t), sizeof handle);
        return handle;
    }

    std::uint64_t pool(std::uint32_t family, std::uint64_t on = device)
    {
        WireWriter writer = frame();
        writer.write_pod(on);
        VkCommandPoolCreateInfo create = {};
        create.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create.queueFamilyIndex = family;
        vkproto::serialize_VkCommandPoolCreateInfo(writer, identity_, create);
        if (!send(vkproto::kCmd_vkCreateCommandPool, writer) || result() != VK_SUCCESS) return 0u;
        return answered_handle();
    }

    std::uint64_t command_buffer(std::uint64_t pool_handle)
    {
        return make(pool_handle, vkproto::HandleKind::command_buffer);
    }

    std::uint64_t buffer() { return make(device, vkproto::HandleKind::buffer); }
    std::uint64_t event() { return make(device, vkproto::HandleKind::event); }
    std::uint64_t semaphore() { return make(device, vkproto::HandleKind::semaphore); }
    std::uint64_t fence() { return make(device, vkproto::HandleKind::fence); }

    bool fill(std::uint64_t command_buffer_handle, std::uint64_t target)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        writer.write_pod(target);
        writer.write_pod<std::uint64_t>(0u);
        writer.write_pod<std::uint64_t>(16u);
        writer.write_pod<std::uint32_t>(7u);
        return send(vkproto::kCmd_vkCmdFillBuffer, writer);
    }

    bool draw(std::uint64_t command_buffer_handle)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        for (std::uint32_t field = 0u; field < 4u; field++) writer.write_pod<std::uint32_t>(1u);
        return send(vkproto::kCmd_vkCmdDraw, writer);
    }

    bool dispatch(std::uint64_t command_buffer_handle)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        for (std::uint32_t field = 0u; field < 3u; field++) writer.write_pod<std::uint32_t>(1u);
        return send(vkproto::kCmd_vkCmdDispatch, writer);
    }

    bool reset_queries(std::uint64_t command_buffer_handle, std::uint64_t queries)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        writer.write_pod(queries);
        writer.write_pod<std::uint32_t>(0u);
        writer.write_pod<std::uint32_t>(1u);
        return send(vkproto::kCmd_vkCmdResetQueryPool, writer);
    }

    bool wait_events(std::uint64_t command_buffer_handle, const std::uint64_t *events, std::uint32_t count,
                     VkPipelineStageFlags source)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        writer.write_pod(count);
        for (std::uint32_t index = 0u; index < count; index++) writer.write_pod(events[index]);
        writer.write_pod(source);
        writer.write_pod<VkPipelineStageFlags>(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        for (std::uint32_t field = 0u; field < 3u; field++) writer.write_pod<std::uint32_t>(0u);
        return send(vkproto::kCmd_vkCmdWaitEvents, writer);
    }

    bool wait_events2(std::uint64_t command_buffer_handle, std::uint64_t event_handle, VkPipelineStageFlags2 source,
                      bool with_barrier)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        writer.write_pod<std::uint32_t>(1u);
        writer.write_pod(event_handle);
        VkMemoryBarrier2 barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        barrier.srcStageMask = source;
        VkDependencyInfo dependency = {};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.memoryBarrierCount = with_barrier ? 1u : 0u;
        dependency.pMemoryBarriers = with_barrier ? &barrier : nullptr;
        vkproto::serialize_VkDependencyInfo(writer, identity_, dependency);
        return send(vkproto::kCmd_vkCmdWaitEvents2, writer);
    }

    bool destroy_device()
    {
        WireWriter writer = frame();
        writer.write_pod(device);
        return send(vkproto::kCmd_vkDestroyDevice, writer);
    }

    bool begin(std::uint64_t command_buffer_handle)
    {
        WireWriter writer = frame();
        writer.write_pod(command_buffer_handle);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkproto::serialize_VkCommandBufferBeginInfo(writer, identity_, info);
        return send(vkproto::kCmd_vkBeginCommandBuffer, writer);
    }

    bool set_event(std::uint64_t event_handle)
    {
        WireWriter writer = frame();
        writer.write_pod(device);
        writer.write_pod(event_handle);
        return send(vkproto::kCmd_vkSetEvent, writer);
    }

    bool destroy_event(std::uint64_t event_handle)
    {
        WireWriter writer = frame();
        writer.write_pod(device);
        writer.write_pod(event_handle);
        return send(vkproto::kCmd_vkDestroyEvent, writer);
    }

    bool submit(std::uint64_t on_queue, std::uint64_t command_buffer_handle, std::uint64_t guest_fence,
                const std::uint64_t *waits = nullptr, std::uint32_t wait_count = 0u,
                const std::uint64_t *signals = nullptr, std::uint32_t signal_count = 0u)
    {
        WireWriter writer = frame();
        writer.write_pod(on_queue);
        writer.write_pod<std::uint32_t>(1u);
        VkSemaphore wait_handles[4] = {};
        VkSemaphore signal_handles[4] = {};
        VkPipelineStageFlags stages[4] = {};
        for (std::uint32_t index = 0u; index < wait_count; index++) {
            wait_handles[index] = reinterpret_cast<VkSemaphore>(waits[index]);
            stages[index] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
        for (std::uint32_t index = 0u; index < signal_count; index++)
            signal_handles[index] = reinterpret_cast<VkSemaphore>(signals[index]);
        const VkCommandBuffer buffers[1] = {reinterpret_cast<VkCommandBuffer>(command_buffer_handle)};
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = wait_count;
        info.pWaitSemaphores = wait_count != 0u ? wait_handles : nullptr;
        info.pWaitDstStageMask = wait_count != 0u ? stages : nullptr;
        info.commandBufferCount = command_buffer_handle != 0u ? 1u : 0u;
        info.pCommandBuffers = command_buffer_handle != 0u ? buffers : nullptr;
        info.signalSemaphoreCount = signal_count;
        info.pSignalSemaphores = signal_count != 0u ? signal_handles : nullptr;
        vkproto::serialize_VkSubmitInfo(writer, identity_, info);
        writer.write_pod(guest_fence);
        return send(vkproto::kCmd_vkQueueSubmit, writer) && result() == VK_SUCCESS;
    }

    vkproto::HandleIdentity &identity() { return identity_; }

private:
    RecordingLog            sink_;
    DriverLog               lines_;
    VulkanBackend           backend_;
    vkproto::HandleIdentity identity_;
    std::uint64_t           made_;
    std::size_t             answered_ = 0u;
    std::uint8_t            block_[1u << 16];
    std::uint8_t            frame_[frame_bytes];
    std::uint8_t            reply_[frame_bytes];
};

}

#endif
