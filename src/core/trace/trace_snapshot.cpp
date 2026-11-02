#include "core/trace/trace_snapshot.hpp"

namespace av {

TraceSnapshot::TraceSnapshot() : bytes_(nullptr), layout_(0u), module_{} {}

Status TraceSnapshot::read(const uint8_t *bytes, size_t size)
{
    if (bytes == nullptr || size < TraceLayout::header_bytes) return Status::malformed;
    const TraceSnapshot held = *this;
    bytes_ = bytes;
    const uint64_t slots = field(TraceLayout::slot_count_at, 4u);
    const TraceLayout layout(static_cast<uint32_t>(slots));
    bool valid = field(TraceLayout::magic_at, 8u) == TraceLayout::magic &&
                 field(TraceLayout::version_at, 4u) == TraceLayout::version &&
                 field(TraceLayout::header_bytes_at, 4u) == TraceLayout::header_bytes &&
                 field(TraceLayout::slot_bytes_at, 4u) == TraceLayout::slot_bytes &&
                 field(TraceLayout::frames_at, 4u) == TraceLayout::frames_per_slot &&
                 field(TraceLayout::frame_bytes_at, 4u) == TraceLayout::frame_bytes &&
                 slots <= TraceLayout::max_slots && layout.bytes() <= size;
    bool terminated = false;
    for (uint32_t byte = 0u; byte < TraceLayout::module_bytes && !terminated; byte++)
        terminated = bytes[TraceLayout::module_at + byte] == 0u;
    if (!valid || !terminated) {
        *this = held;
        return Status::malformed;
    }
    layout_ = layout;
    for (uint32_t byte = 0u; byte < TraceLayout::module_bytes; byte++)
        module_[byte] = static_cast<char>(bytes[TraceLayout::module_at + byte]);
    return Status::ok;
}

uint32_t TraceSnapshot::slots() const { return layout_.slots(); }

TraceSlotView TraceSnapshot::slot(uint32_t index) const
{
    if (index >= layout_.slots()) return TraceSlotView();
    return TraceSlotView(bytes_ + layout_.slot(index), TraceLayout::frames_per_slot);
}

const char *TraceSnapshot::module() const { return module_; }
uint32_t    TraceSnapshot::process() const { return static_cast<uint32_t>(field(TraceLayout::process_at, 4u)); }
uint64_t    TraceSnapshot::created() const { return field(TraceLayout::created_at, 8u); }
uint32_t    TraceSnapshot::crash_code() const { return static_cast<uint32_t>(field(TraceLayout::crash_code_at, 4u)); }
uint64_t    TraceSnapshot::crash_address() const { return field(TraceLayout::crash_address_at, 8u); }
uint32_t    TraceSnapshot::crash_thread() const { return static_cast<uint32_t>(field(TraceLayout::crash_thread_at, 4u)); }
uint64_t    TraceSnapshot::crashed_at() const { return field(TraceLayout::crashed_at, 8u); }

bool TraceSnapshot::invariants_hold() const
{
    if (bytes_ == nullptr) return layout_.slots() == 0u && module_[0] == '\0';
    for (uint32_t byte = 0u; byte < TraceLayout::module_bytes; byte++)
        if (module_[byte] == '\0') return true;
    return false;
}

uint64_t TraceSnapshot::field(uint32_t offset, uint32_t bytes) const
{
    if (bytes_ == nullptr) return 0u;
    uint64_t value = 0u;
    for (uint32_t byte = 0u; byte < bytes; byte++) value |= static_cast<uint64_t>(bytes_[offset + byte]) << (8u * byte);
    return value;
}

}
