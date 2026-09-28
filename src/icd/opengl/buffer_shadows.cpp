#include "icd/opengl/buffer_shadows.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

BufferShadows::BufferShadows(BlockSource &memory)
    : memory_(memory), shadows_{}, fresh_{}, next_fresh_(0u), size_names_{}, sizes_{}, next_size_(0u),
      held_bytes_(0u), clock_(0u), distrusted_(false)
{
    AV_SCOPE("BufferShadows::BufferShadows");
}

BufferShadows::~BufferShadows()
{
    AV_SCOPE("BufferShadows::~BufferShadows");
    forget_all();
}

uint8_t *BufferShadows::covering(uint32_t buffer, uint64_t offset, uint64_t length)
{
    AV_SCOPE("BufferShadows::covering");
    if (let_go()) return nullptr;
    const uint32_t slot = holding(buffer);
    if (slot == max_shadows || !shadows_[slot].object().covers(buffer, offset, length)) return nullptr;
    return shadows_[slot].object().at(offset, ++clock_);
}

void BufferShadows::keep(uint32_t buffer, uint64_t offset, uint64_t length, const uint8_t *bytes)
{
    AV_SCOPE("BufferShadows::keep");
    if (let_go() || buffer == 0u || length == 0u || length > max_bytes) return;
    unmark(buffer);
    const uint32_t slot = holding(buffer);
    if (slot != max_shadows && shadows_[slot].object().covers(buffer, offset, length)) {
        __builtin_memcpy(shadows_[slot].object().at(offset, ++clock_), bytes, static_cast<size_t>(length));
        return;
    }
    if (slot != max_shadows) drop(slot);
    const uint32_t free = make_room(length);
    static_cast<void>(shadows_[free].construct(memory_));
    if (shadows_[free].object().hold(buffer, offset, length, bytes, ++clock_) != Status::ok) {
        shadows_[free].destroy();
        return;
    }
    held_bytes_ += length;
}

void BufferShadows::wrote(uint32_t buffer, uint64_t offset, uint64_t length, const void *bytes)
{
    AV_SCOPE("BufferShadows::wrote");
    unmark(buffer);
    const uint32_t slot = holding(buffer);
    if (slot == max_shadows) return;
    /* No bytes named: what the host does with that is not the guest's to
     * guess. */
    if (bytes == nullptr) {
        drop(slot);
        return;
    }
    shadows_[slot].object().overwrite(offset, length, static_cast<const uint8_t *>(bytes));
}

void BufferShadows::respecify(uint32_t buffer, int64_t size, bool with_data)
{
    AV_SCOPE("BufferShadows::respecify");
    if (size < 0) return;
    forget(buffer);
    sized(buffer, static_cast<uint64_t>(size));
    if (with_data || let_go() || buffer == 0u) return;
    fresh_[next_fresh_] = buffer;
    next_fresh_ = (next_fresh_ + 1u) % max_fresh;
}

void BufferShadows::sized(uint32_t buffer, uint64_t size)
{
    AV_SCOPE("BufferShadows::sized");
    if (let_go() || buffer == 0u) return;
    uint32_t row = size_row(buffer);
    if (row == max_sizes) {
        row = next_size_;
        next_size_ = (next_size_ + 1u) % max_sizes;
    }
    size_names_[row] = buffer;
    sizes_[row] = size;
}

bool BufferShadows::size_of(uint32_t buffer, uint64_t &size)
{
    AV_SCOPE("BufferShadows::size_of");
    const uint32_t row = size_row(buffer);
    if (let_go() || buffer == 0u || row == max_sizes) return false;
    size = sizes_[row];
    return true;
}

bool BufferShadows::fresh(uint32_t buffer) const
{
    AV_SCOPE("BufferShadows::fresh");
    if (buffer == 0u || !trusted()) return false;
    for (const uint32_t marked : fresh_)
        if (marked == buffer) return true;
    return false;
}

void BufferShadows::forget(uint32_t buffer)
{
    AV_SCOPE("BufferShadows::forget");
    unmark(buffer);
    const uint32_t row = size_row(buffer);
    if (row != max_sizes) size_names_[row] = 0u;
    const uint32_t slot = holding(buffer);
    if (slot != max_shadows) drop(slot);
}

void BufferShadows::forget(int32_t count, const uint32_t *buffers)
{
    AV_SCOPE("BufferShadows::forget");
    if (buffers == nullptr) return;
    for (int32_t at = 0; at < count; at++) forget(buffers[at]);
}

void BufferShadows::forget_all()
{
    AV_SCOPE("BufferShadows::forget_all");
    for (uint32_t slot = 0u; slot < max_shadows; slot++) {
        if (shadows_[slot].constructed()) drop(slot);
    }
    for (uint32_t &marked : fresh_) marked = 0u;
    for (uint32_t &named : size_names_) named = 0u;
}

void BufferShadows::distrust()
{
    AV_SCOPE("BufferShadows::distrust");
    __atomic_store_n(&distrusted_, true, __ATOMIC_RELEASE);
}

bool BufferShadows::trusted() const
{
    AV_SCOPE("BufferShadows::trusted");
    return !__atomic_load_n(&distrusted_, __ATOMIC_ACQUIRE);
}

uint32_t BufferShadows::held() const
{
    AV_SCOPE("BufferShadows::held");
    uint32_t count = 0u;
    for (const ObjectStorage<BufferShadow> &shadow : shadows_) count += shadow.constructed() ? 1u : 0u;
    return count;
}

void BufferShadows::report(StateWriter &writer) const
{
    AV_SCOPE("BufferShadows::report");
    writer.open("shadows", 0u);
    writer.flag("trusted", trusted());
    writer.number("held", held());
    writer.number("held_bytes", held_bytes_);
    uint32_t sizes = 0u;
    for (const uint32_t named : size_names_) sizes += named != 0u ? 1u : 0u;
    writer.number("sizes", sizes);
    for (uint32_t slot = 0u; slot < max_shadows; slot++) {
        if (shadows_[slot].constructed()) shadows_[slot].object().report(writer, slot);
    }
    writer.close();
}

/* Each copy is whole and of a buffer no other copy is, their bytes are counted
 * and within the budget, and no buffer has two sizes. A distrusted context can
 * still hold what it had until its own thread next asks. */
bool BufferShadows::invariants_hold() const
{
    AV_SCOPE("BufferShadows::invariants_hold");
    uint64_t bytes = 0u;
    for (uint32_t slot = 0u; slot < max_shadows; slot++) {
        if (!shadows_[slot].constructed()) continue;
        const BufferShadow &shadow = shadows_[slot].object();
        if (!shadow.invariants_hold() || shadow.buffer() == 0u || holding(shadow.buffer()) != slot) return false;
        bytes += shadow.length();
    }
    for (uint32_t row = 0u; row < max_sizes; row++)
        if (size_names_[row] != 0u && size_row(size_names_[row]) != row) return false;
    return bytes == held_bytes_ && held_bytes_ <= max_bytes && next_fresh_ < max_fresh && next_size_ < max_sizes;
}

uint32_t BufferShadows::holding(uint32_t buffer) const
{
    AV_SCOPE("BufferShadows::holding");
    for (uint32_t slot = 0u; slot < max_shadows; slot++) {
        if (shadows_[slot].constructed() && shadows_[slot].object().buffer() == buffer) return slot;
    }
    return max_shadows;
}

uint32_t BufferShadows::free_slot() const
{
    AV_SCOPE("BufferShadows::free_slot");
    for (uint32_t slot = 0u; slot < max_shadows; slot++) {
        if (!shadows_[slot].constructed()) return slot;
    }
    return max_shadows;
}

uint32_t BufferShadows::least_recent() const
{
    AV_SCOPE("BufferShadows::least_recent");
    uint32_t oldest = max_shadows;
    for (uint32_t slot = 0u; slot < max_shadows; slot++) {
        if (!shadows_[slot].constructed()) continue;
        if (oldest == max_shadows || shadows_[slot].object().stamp() < shadows_[oldest].object().stamp())
            oldest = slot;
    }
    return oldest;
}

/* Bytes over the budget mean a copy is held, and no free slot means every
 * slot is, so there is always one to drop. */
uint32_t BufferShadows::make_room(uint64_t length)
{
    AV_SCOPE("BufferShadows::make_room");
    while (held_bytes_ + length > max_bytes || free_slot() == max_shadows) drop(least_recent());
    return free_slot();
}

void BufferShadows::drop(uint32_t slot)
{
    AV_SCOPE("BufferShadows::drop");
    held_bytes_ -= shadows_[slot].object().length();
    shadows_[slot].destroy();
}

/* Another context's thread may distrust this one's copies while its own thread
 * uses them, so only the flag crosses; what is held goes here, on the thread
 * that holds it. */
bool BufferShadows::let_go()
{
    AV_SCOPE("BufferShadows::let_go");
    if (trusted()) return false;
    forget_all();
    return true;
}

uint32_t BufferShadows::size_row(uint32_t buffer) const
{
    AV_SCOPE("BufferShadows::size_row");
    for (uint32_t row = 0u; row < max_sizes; row++)
        if (size_names_[row] == buffer) return row;
    return max_sizes;
}

void BufferShadows::unmark(uint32_t buffer)
{
    AV_SCOPE("BufferShadows::unmark");
    for (uint32_t &marked : fresh_)
        if (marked == buffer) marked = 0u;
}

}  // namespace av::glicd
