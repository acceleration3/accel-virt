#include "executor/shim/frame_buffer_copy.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/trace/function_scope.hpp"
#include "executor/owned_fd.hpp"
#include "executor/shim/borrowed_queue.hpp"
#include "executor/shim/vulkan_device_table.hpp"

#include <sched.h>
#include <stdint.h>

namespace av {

namespace {

constexpr uint32_t   texel_bytes = 4u;
constexpr uint64_t   nanoseconds_per_ms = 1000000u;
/* A buffer waited for before it goes: a copy not done in a second is a GPU
 * that is gone. */
constexpr uint64_t   most_wait_nanoseconds = 1000000000u;

bool four_byte_colour(VkFormat format)
{
    AV_SCOPE("four_byte_colour");
    return format == VK_FORMAT_B8G8R8A8_UNORM || format == VK_FORMAT_B8G8R8A8_SRGB ||
           format == VK_FORMAT_R8G8B8A8_UNORM || format == VK_FORMAT_R8G8B8A8_SRGB ||
           format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 || format == VK_FORMAT_A2R10G10B10_UNORM_PACK32;
}

}  // namespace

FrameBufferCopy::FrameBufferCopy()
    : table_(nullptr), lock_(), timeline_(), work_(), imports_{}, value_(0u), clock_(0u), waiters_(0u), closing_(0u), epoch_(0u)
{
    AV_SCOPE("FrameBufferCopy::FrameBufferCopy");
}

FrameBufferCopy::~FrameBufferCopy()
{
    AV_SCOPE("FrameBufferCopy::~FrameBufferCopy");
    close();
}

bool FrameBufferCopy::open(const VulkanDeviceTable &table, BorrowedQueue &queue)
{
    AV_SCOPE("FrameBufferCopy::open");
    const ThreadLockHold held(lock_);
    if (opened()) return true;
    if (!timeline_.make_local(table) || !work_.open(table, queue)) {
        timeline_.release();
        return false;
    }
    table_ = &table;
    __atomic_fetch_add(&epoch_, 1u, __ATOMIC_ACQ_REL);
    __atomic_store_n(&closing_, 0u, __ATOMIC_RELEASE);
    return true;
}

bool FrameBufferCopy::opened() const
{
    AV_SCOPE("FrameBufferCopy::opened");
    return table_ != nullptr;
}

/* A thread in wait() is on the timeline, which goes with this: it is turned
 * away from now on and the ones inside are waited out - each is bounded by
 * its own wait_ms - before the timeline is let go. The queue is idle before
 * any import goes, so nothing is freed under a copy. */
void FrameBufferCopy::close()
{
    AV_SCOPE("FrameBufferCopy::close");
    __atomic_store_n(&closing_, 1u, __ATOMIC_SEQ_CST);
    while (__atomic_load_n(&waiters_, __ATOMIC_SEQ_CST) != 0u) sched_yield();
    const ThreadLockHold held(lock_);
    if (!opened()) return;
    work_.idle();
    for (Import &import : imports_) release(import);
    work_.close();
    timeline_.release();
    table_ = nullptr;
    value_ = 0u;
    clock_ = 0u;
}

void FrameBufferCopy::release(Import &import)
{
    AV_SCOPE("FrameBufferCopy::release");
    import.buffer_.release();
    import.lending_ = 0u;
    import.value_ = 0u;
}

/* Never freed under a copy that has not finished: an import whose last copy
 * is not reached in a second stays, and the caller declines. */
bool FrameBufferCopy::forget(Import &import)
{
    AV_SCOPE("FrameBufferCopy::forget");
    if (!timeline_.wait(import.value_, most_wait_nanoseconds)) return false;
    release(import);
    return true;
}

FrameBufferCopy::Import *FrameBufferCopy::room(uint64_t oldest_live)
{
    AV_SCOPE("FrameBufferCopy::room");
    Import *oldest = nullptr;
    for (Import &import : imports_) {
        if (!import.buffer_.made()) return &import;
        if (import.lending_ < oldest_live && forget(import)) return &import;
        if (oldest == nullptr || import.used_ < oldest->used_) oldest = &import;
    }
    return forget(*oldest) ? oldest : nullptr;
}

/* Imports of lendings given back are dropped as they are found, so a dead
 * lending's gigabyte does not wait for the cache to fill. */
void FrameBufferCopy::drop_dead(uint64_t oldest_live)
{
    AV_SCOPE("FrameBufferCopy::drop_dead");
    for (Import &import : imports_)
        if (import.buffer_.made() && import.lending_ < oldest_live) static_cast<void>(forget(import));
}

FrameBufferCopy::Import *FrameBufferCopy::import_for(const AvFrameCopy &target)
{
    AV_SCOPE("FrameBufferCopy::import_for");
    drop_dead(target.oldest_live);
    for (Import &import : imports_)
        if (import.buffer_.made() && import.lending_ == target.lending) return &import;
    OwnedFd descriptor;
    if (target.dma_buf < 0 || !descriptor.hold_copy(target.dma_buf)) return nullptr;
    Import *const slot = room(target.oldest_live);
    if (slot == nullptr || !slot->buffer_.import_dma_buf_to_receive(*table_, descriptor, target.size)) return nullptr;
    slot->lending_ = target.lending;
    return slot;
}

/* Whether two regions' rectangles in the buffer share a byte; the layout
 * forbids it, and the CPU would have let the later region win. */
bool FrameBufferCopy::overlap(const AvFrameRegion &first, const AvFrameRegion &second) const
{
    AV_SCOPE("FrameBufferCopy::overlap");
    return uint64_t{first.destination_x} < uint64_t{second.destination_x} + second.width &&
           uint64_t{second.destination_x} < uint64_t{first.destination_x} + first.width &&
           uint64_t{first.destination_y} < uint64_t{second.destination_y} + second.height &&
           uint64_t{second.destination_y} < uint64_t{first.destination_y} + first.height;
}

/* Rows of the region land `pitch` bytes apart from the byte its first texel
 * is at, in `pitch / 4` texels. Every product is of values already bounded by
 * the buffer: a row is counted before it is multiplied, a region stays inside
 * its rows, and a region whose row would run into the next is declined, as is
 * one that overlaps an earlier one. */
bool FrameBufferCopy::place(const VkImageCreateInfo &described, const AvFrameCopy &target,
                            VkBufferImageCopy *placed) const
{
    AV_SCOPE("FrameBufferCopy::place");
    const uint32_t width = described.extent.width, height = described.extent.height;
    if (!four_byte_colour(described.format) || described.samples != VK_SAMPLE_COUNT_1_BIT ||
        (described.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0u || target.pitch == 0u ||
        target.pitch > static_cast<uint32_t>(INT32_MAX) || target.pitch % texel_bytes != 0u ||
        (target.regions == nullptr && target.count != 0u) || target.count > AV_FRAME_COPY_MOST_REGIONS ||
        target.offset > target.size || target.offset % texel_bytes != 0u)
        return false;
    const uint64_t rows = (target.size - target.offset) / target.pitch;
    for (uint32_t index = 0u; index < target.count; index++) {
        const AvFrameRegion &region = target.regions[index];
        if (region.width == 0u || region.height == 0u || (uint64_t{region.width} + region.destination_x) * texel_bytes > target.pitch ||
            uint64_t{region.source_x} + region.width > width || uint64_t{region.source_y} + region.height > height ||
            region.destination_y >= rows || region.height > rows - region.destination_y)
            return false;
        for (uint32_t before = 0u; before < index; before++)
            if (overlap(target.regions[before], region)) return false;
        const uint64_t first = target.offset + uint64_t{region.destination_y} * target.pitch +
                               uint64_t{region.destination_x} * texel_bytes;
        placed[index] = VkBufferImageCopy{first,
                                          target.pitch / texel_bytes,
                                          0u,
                                          {VK_IMAGE_ASPECT_COLOR_BIT, 0u, 0u, 1u},
                                          {static_cast<int32_t>(region.source_x), static_cast<int32_t>(region.source_y), 0},
                                          {region.width, region.height, 1u}};
    }
    return true;
}

/* A copy into a lending waits for the last one into it: they would write the
 * same bytes in the order they were asked, and a late one, whose present was
 * refused, must not land over this one. */
bool FrameBufferCopy::submit(VkImage image, VkImageLayout layout, const VkImageCreateInfo &described,
                             const AvFrameCopy &target, uint64_t &value)
{
    AV_SCOPE("FrameBufferCopy::submit");
    VkBufferImageCopy placed[AV_FRAME_COPY_MOST_REGIONS];
    const ThreadLockHold held(lock_);
    if (!opened() || !place(described, target, placed)) return false;
    value = 0u;
    if (target.count == 0u) return true;
    Import *const import = import_for(target);
    if (import == nullptr || !timeline_.wait(import->value_, most_wait_nanoseconds)) return false;
    const uint64_t next = value_ + 1u;
    if (!work_.copy_to_buffer(image, layout, import->buffer_.buffer(), placed, target.count, timeline_, next))
        return false;
    value_ = next;
    import->value_ = next;
    import->used_ = ++clock_;
    value = next;
    return true;
}

bool FrameBufferCopy::wait(uint64_t value, uint32_t wait_ms) const
{
    AV_SCOPE("FrameBufferCopy::wait");
    __atomic_fetch_add(&waiters_, 1u, __ATOMIC_SEQ_CST);
    const bool reached = __atomic_load_n(&closing_, __ATOMIC_SEQ_CST) == 0u &&
                         timeline_.wait(value, uint64_t{wait_ms} * nanoseconds_per_ms);
    __atomic_fetch_sub(&waiters_, 1u, __ATOMIC_SEQ_CST);
    return reached;
}

uint32_t FrameBufferCopy::epoch() const
{
    AV_SCOPE("FrameBufferCopy::epoch");
    return __atomic_load_n(&epoch_, __ATOMIC_ACQUIRE);
}

bool FrameBufferCopy::finished(uint32_t epoch, uint64_t value, uint32_t wait_ms) const
{
    AV_SCOPE("FrameBufferCopy::finished");
    return value == 0u || !opened() || epoch != this->epoch() || wait(value, wait_ms);
}

}  // namespace av
