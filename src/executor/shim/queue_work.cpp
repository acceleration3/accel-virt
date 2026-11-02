#include "executor/shim/queue_work.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/borrowed_queue.hpp"
#include "executor/shim/vulkan_device_table.hpp"
#include "executor/shim/vulkan_timeline.hpp"

namespace av {

namespace {

constexpr uint64_t most_wait_nanoseconds = 1000000000u;

}

QueueWork::QueueWork()
    : table_(nullptr), queue_(nullptr), pool_(VK_NULL_HANDLE), buffers_{}, signals_{}, values_{}, next_(0u)
{
    AV_SCOPE("QueueWork::QueueWork");
}

QueueWork::~QueueWork()
{
    AV_SCOPE("QueueWork::~QueueWork");
    close();
}

bool QueueWork::open(const VulkanDeviceTable &table, BorrowedQueue &queue)
{
    AV_SCOPE("QueueWork::open");
    if (opened()) return true;
    if (!table.create_command_pool(queue.family(), pool_)) return false;
    table_ = &table;
    queue_ = &queue;
    if (table.allocate_command_buffers(pool_, in_flight, buffers_)) return true;
    close();
    return false;
}

bool QueueWork::opened() const
{
    AV_SCOPE("QueueWork::opened");
    return pool_ != VK_NULL_HANDLE;
}

void QueueWork::close()
{
    AV_SCOPE("QueueWork::close");
    if (pool_ != VK_NULL_HANDLE) {
        idle();
        table_->destroy_command_pool(pool_);
    }
    pool_ = VK_NULL_HANDLE;
    table_ = nullptr;
    queue_ = nullptr;
    for (uint32_t slot = 0u; slot < in_flight; slot++) {
        buffers_[slot] = VK_NULL_HANDLE;
        signals_[slot] = nullptr;
        values_[slot] = 0u;
    }
    next_ = 0u;
}

void QueueWork::idle()
{
    AV_SCOPE("QueueWork::idle");
    if (queue_ == nullptr) return;
    queue_->flush();
    queue_->lock();
    table_->queue_idle(queue_->queue());
    queue_->unlock();
}

VkCommandBuffer QueueWork::next_buffer()
{
    AV_SCOPE("QueueWork::next_buffer");
    const uint32_t slot = next_;
    next_ = (next_ + 1u) % in_flight;
    if (signals_[slot] != nullptr) static_cast<void>(signals_[slot]->wait(values_[slot], most_wait_nanoseconds));
    signals_[slot] = nullptr;
    table_->reset(buffers_[slot]);
    return buffers_[slot];
}

void QueueWork::record(VkCommandBuffer buffer, VkImage source, VkImageLayout layout, VkImage destination,
                       uint32_t width, uint32_t height) const
{
    AV_SCOPE("QueueWork::record");
    const VkImageSubresourceRange whole = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 1u, 0u, 1u};
    const VkImageMemoryBarrier    before[2] = {
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
         layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, source, whole},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0u, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, destination, whole}};
    table_->barrier(buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 2u, before);
    const VkImageSubresourceLayers layer = {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u};
    table_->copy_image(buffer, source, destination, VkImageCopy{layer, {0, 0, 0}, layer, {0, 0, 0}, {width, height, 1u}});
    const VkImageMemoryBarrier after[2] = {
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_READ_BIT,
         VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout,
         VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, source, whole},
        {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED, destination, whole}};
    table_->barrier(buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 2u, after);
}

bool QueueWork::copy(VkImage source, VkImageLayout layout, VkImage destination, uint32_t width, uint32_t height,
                     const VulkanTimeline &signal, uint64_t value)
{
    AV_SCOPE("QueueWork::copy");
    if (!opened() || source == VK_NULL_HANDLE || destination == VK_NULL_HANDLE || !signal.made()) return false;
    queue_->flush();
    const uint32_t        slot = next_;
    const VkCommandBuffer buffer = next_buffer();
    if (!table_->begin(buffer)) return false;
    record(buffer, source, layout, destination, width, height);
    return send(slot, buffer, signal, value);
}

bool QueueWork::send(uint32_t slot, VkCommandBuffer buffer, const VulkanTimeline &signal, uint64_t value)
{
    AV_SCOPE("QueueWork::send");
    if (!table_->end(buffer)) return false;
    const VkSemaphore                   semaphore = signal.semaphore();
    const VkTimelineSemaphoreSubmitInfo timed = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr, 0u,
                                                 nullptr, 1u, &value};
    const VkSubmitInfo submitted = {VK_STRUCTURE_TYPE_SUBMIT_INFO, &timed, 0u, nullptr, nullptr, 1u, &buffer, 1u,
                                    &semaphore};
    queue_->lock();
    const bool sent = table_->submit(queue_->queue(), submitted);
    queue_->unlock();
    if (!sent) return false;
    signals_[slot] = &signal;
    values_[slot] = value;
    return true;
}

bool QueueWork::signal(const VulkanTimeline &timeline, uint64_t value)
{
    AV_SCOPE("QueueWork::signal");
    if (queue_ == nullptr || !timeline.made()) return false;
    queue_->flush();
    const VkSemaphore semaphore = timeline.semaphore();
    const VkTimelineSemaphoreSubmitInfo values = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr, 0u,
                                                  nullptr, 1u, &value};
    const VkSubmitInfo submitted = {VK_STRUCTURE_TYPE_SUBMIT_INFO, &values, 0u, nullptr, nullptr, 0u, nullptr, 1u,
                                    &semaphore};
    queue_->lock();
    const bool sent = table_->submit(queue_->queue(), submitted);
    queue_->unlock();
    return sent;
}

bool QueueWork::wait(const VulkanTimeline &timeline, uint64_t value)
{
    AV_SCOPE("QueueWork::wait");
    if (queue_ == nullptr || !timeline.made()) return false;
    queue_->flush();
    const VkSemaphore                   semaphore = timeline.semaphore();
    const VkPipelineStageFlags          stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    const VkTimelineSemaphoreSubmitInfo values = {VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO, nullptr, 1u,
                                                  &value, 0u, nullptr};
    const VkSubmitInfo submitted = {VK_STRUCTURE_TYPE_SUBMIT_INFO, &values, 1u, &semaphore, &stage, 0u, nullptr, 0u,
                                    nullptr};
    queue_->lock();
    const bool sent = table_->submit(queue_->queue(), submitted);
    queue_->unlock();
    return sent;
}

}
