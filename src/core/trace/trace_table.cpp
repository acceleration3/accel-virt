#include "core/trace/trace_table.hpp"

#include "core/trace/scope_name.hpp"
#include "core/trace/trace_host.hpp"

namespace av {

namespace {

/* A thread key's bits spread over the slots: keys are pointers or small ids,
 * both with structure in their low bits. */
constexpr uint64_t key_mix = 0x9E3779B97F4A7C15ull;
constexpr uint32_t mix_shift = 32u;

}  // namespace

TraceTable::TraceTable(TraceHost &host) : host_(host), region_(nullptr), layout_(0u) {}

Status TraceTable::initialize(uint8_t *region, size_t bytes, const char *module, uint32_t process)
{
    if (region_ != nullptr) return Status::invalid_state;
    const TraceLayout fit(bytes);
    if (region == nullptr || fit.slots() == 0u || module == nullptr) return Status::invalid_parameter;
    if (reinterpret_cast<uintptr_t>(region) % alignof(uint64_t) != 0u) return Status::invalid_parameter;
    size_t length = 0u;
    while (module[length] != '\0') {
        if (++length == TraceLayout::module_bytes) return Status::invalid_parameter;
    }
    for (size_t byte = 0u; byte < fit.bytes(); byte++) region[byte] = 0u;
    region_ = region;
    layout_ = fit;
    for (size_t byte = 0u; byte < length; byte++) region_[TraceLayout::module_at + byte] = static_cast<uint8_t>(module[byte]);
    write32(TraceLayout::version_at, TraceLayout::version);
    write32(TraceLayout::header_bytes_at, TraceLayout::header_bytes);
    write32(TraceLayout::slot_count_at, layout_.slots());
    write32(TraceLayout::slot_bytes_at, TraceLayout::slot_bytes);
    write32(TraceLayout::frames_at, TraceLayout::frames_per_slot);
    write32(TraceLayout::frame_bytes_at, TraceLayout::frame_bytes);
    write32(TraceLayout::process_at, process);
    write64(TraceLayout::created_at, host_.clock());
    write64(TraceLayout::ticks_at, TraceLayout::ticks_per_second);
    /* Last: a reader takes the table as laid out from the moment the magic is there. */
    write64(TraceLayout::magic_at, TraceLayout::magic);
    return Status::ok;
}

uint32_t TraceTable::enter(const ScopeName &name)
{
    if (region_ == nullptr) return no_slot;
    const uint64_t key = host_.thread_key();
    if (key == 0u || (key & TraceLayout::active) != 0u) return no_slot;
    uint32_t slot = active_slot(key);
    if (slot == no_slot) slot = claim(key);
    if (slot == no_slot) return no_slot;
    push(slot, name);
    return slot;
}

void TraceTable::leave(uint32_t slot)
{
    if (region_ == nullptr || slot >= layout_.slots()) return;
    const uint64_t key = host_.thread_key();
    const size_t   at = layout_.slot(slot);
    if (owner(slot) != (key | TraceLayout::active)) return;
    /* An active slot holds at least the frame its enter pushed. */
    const uint32_t depth = read32(at + TraceLayout::depth_at);
    write32(at + TraceLayout::depth_at, depth - 1u);
    if (depth != 1u) return;
    write32(at + TraceLayout::last_left_at, read32(layout_.frame(slot, 0u) + TraceLayout::name_at));
    write64(at + TraceLayout::left_at, host_.clock());
    write64(at + TraceLayout::waiting_at, 0u);
    /* Only this thread moves its own slot out of active, so the swap cannot
     * meet another writer; a failure would leave the slot active, which the
     * next entry of this thread reuses. */
    static_cast<void>(host_.exchange(owner(slot), key | TraceLayout::active, key));
}

void TraceTable::waiting(uint64_t lock)
{
    if (region_ == nullptr) return;
    const uint32_t slot = active_slot(host_.thread_key());
    if (slot == no_slot) return;
    write64(layout_.slot(slot) + TraceLayout::waiting_at, lock);
}

void TraceTable::acquired(uint64_t lock)
{
    if (region_ == nullptr) return;
    const uint32_t slot = active_slot(host_.thread_key());
    if (slot == no_slot) return;
    write64(layout_.slot(slot) + TraceLayout::waiting_at, 0u);
    write64(layout_.slot(slot) + TraceLayout::holding_at, lock);
}

void TraceTable::released(uint64_t lock)
{
    if (region_ == nullptr) return;
    const uint32_t slot = active_slot(host_.thread_key());
    if (slot == no_slot || read64(layout_.slot(slot) + TraceLayout::holding_at) != lock) return;
    write64(layout_.slot(slot) + TraceLayout::holding_at, 0u);
}

void TraceTable::withdraw()
{
    region_ = nullptr;
    layout_ = TraceLayout(uint32_t{0});
}

void TraceTable::record_crash(uint32_t code, uint64_t address)
{
    if (region_ == nullptr || read64(TraceLayout::crashed_at) != 0u) return;
    write32(TraceLayout::crash_code_at, code);
    write64(TraceLayout::crash_address_at, address);
    write32(TraceLayout::crash_thread_at, host_.thread_number());
    const uint64_t now = host_.clock();
    write64(TraceLayout::crashed_at, now != 0u ? now : 1u);
}

const uint8_t *TraceTable::region() const { return region_; }
size_t         TraceTable::bytes() const { return region_ != nullptr ? layout_.bytes() : 0u; }

bool TraceTable::invariants_hold() const
{
    if (region_ == nullptr) return layout_.slots() == 0u;
    if (read64(TraceLayout::magic_at) != TraceLayout::magic || read32(TraceLayout::slot_count_at) != layout_.slots())
        return false;
    for (uint32_t slot = 0u; slot < layout_.slots(); slot++) {
        const uint64_t word = owner_word(slot);
        const uint32_t depth = read32(layout_.slot(slot) + TraceLayout::depth_at);
        /* A free or idle slot has left every function it entered. */
        if ((word & TraceLayout::active) == 0u && depth != 0u) return false;
        if (word == TraceLayout::active) return false;
    }
    return true;
}

uint64_t TraceTable::read64(size_t offset) const
{
    uint64_t value = 0u;
    for (size_t byte = 0u; byte < sizeof value; byte++)
        value |= static_cast<uint64_t>(region_[offset + byte]) << (8u * byte);
    return value;
}

uint32_t TraceTable::read32(size_t offset) const { return static_cast<uint32_t>(read64(offset) & 0xFFFFFFFFull); }

void TraceTable::write64(size_t offset, uint64_t value)
{
    for (size_t byte = 0u; byte < sizeof value; byte++)
        region_[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

void TraceTable::write32(size_t offset, uint32_t value)
{
    for (size_t byte = 0u; byte < sizeof value; byte++)
        region_[offset + byte] = static_cast<uint8_t>(value >> (8u * byte));
}

void TraceTable::write16(size_t offset, uint16_t value)
{
    region_[offset] = static_cast<uint8_t>(value);
    region_[offset + 1u] = static_cast<uint8_t>(value >> 8u);
}

/* The owner word is swapped by other threads, so it is the one field read and
 * written as a whole u64: a byte-wise read could combine two owners. The
 * region is 8-byte aligned and every slot starts at an 8-byte offset. */
uint64_t &TraceTable::owner(uint32_t slot)
{
    return *static_cast<uint64_t *>(static_cast<void *>(region_ + layout_.slot(slot) + TraceLayout::owner_at));
}

uint64_t TraceTable::owner_word(uint32_t slot) const
{
    return *static_cast<const uint64_t *>(
        static_cast<const void *>(region_ + layout_.slot(slot) + TraceLayout::owner_at));
}

uint32_t TraceTable::first_probe(uint64_t key) const
{
    return static_cast<uint32_t>(((key * key_mix) >> mix_shift) % layout_.slots());
}

uint32_t TraceTable::active_slot(uint64_t key) const
{
    if (key == 0u) return no_slot;
    const uint32_t start = first_probe(key);
    for (uint32_t step = 0u; step < layout_.slots(); step++) {
        const uint32_t slot = (start + step) % layout_.slots();
        if (owner_word(slot) == (key | TraceLayout::active)) return slot;
    }
    return no_slot;
}

/* This thread's idle slot first, so its last function stays with it; then a
 * free one; then another thread's idle slot, whose history is lost. */
uint32_t TraceTable::claim(uint64_t key)
{
    const uint32_t start = first_probe(key);
    for (uint32_t pass = 0u; pass < 3u; pass++) {
        for (uint32_t step = 0u; step < layout_.slots(); step++) {
            const uint32_t slot = (start + step) % layout_.slots();
            const uint64_t word = owner_word(slot);
            const bool     wanted = pass == 0u ? word == key
                                  : pass == 1u ? word == 0u
                                               : (word & TraceLayout::active) == 0u;
            if (wanted && take(slot, word, key)) return slot;
        }
    }
    return no_slot;
}

bool TraceTable::take(uint32_t slot, uint64_t expected, uint64_t key)
{
    if (!host_.exchange(owner(slot), expected, key | TraceLayout::active)) return false;
    const size_t at = layout_.slot(slot);
    write32(at + TraceLayout::depth_at, 0u);
    write32(at + TraceLayout::thread_at, host_.thread_number());
    write64(at + TraceLayout::waiting_at, 0u);
    if (expected == key) return true;
    write64(at + TraceLayout::entries_at, 0u);
    write32(at + TraceLayout::last_left_at, 0u);
    write64(at + TraceLayout::left_at, 0u);
    write64(at + TraceLayout::holding_at, 0u);
    return true;
}

/* The depth is raised before the frame is written: an interrupt on this
 * thread between the two pushes above this frame, not over it. */
void TraceTable::push(uint32_t slot, const ScopeName &name)
{
    const size_t   at = layout_.slot(slot);
    const uint32_t depth = read32(at + TraceLayout::depth_at);
    write32(at + TraceLayout::depth_at, depth + 1u);
    write64(at + TraceLayout::entries_at, read64(at + TraceLayout::entries_at) + 1u);
    if (depth >= TraceLayout::frames_per_slot) return;
    const size_t frame = layout_.frame(slot, depth);
    write32(frame + TraceLayout::name_at, name.id());
    write16(frame + TraceLayout::level_at, host_.level());
    write64(frame + TraceLayout::entered_at, host_.clock());
    write64(frame + TraceLayout::text_at, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(name.text())));
}

}  // namespace av
