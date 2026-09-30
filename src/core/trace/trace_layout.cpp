#include "core/trace/trace_layout.hpp"

#include "core/support/byte_range.hpp"

namespace av {

TraceLayout::TraceLayout(uint32_t slots) : slots_(slots < max_slots ? slots : static_cast<uint32_t>(max_slots)) {}

TraceLayout::TraceLayout(const ByteRange &region) : slots_(0u)
{
    const uint64_t bytes = region.size();
    if (bytes < header_bytes) return;
    const uint64_t fit = (bytes - header_bytes) / slot_bytes;
    slots_ = fit < max_slots ? static_cast<uint32_t>(fit) : static_cast<uint32_t>(max_slots);
}

uint32_t TraceLayout::slots() const { return slots_; }
size_t   TraceLayout::bytes() const { return header_bytes + static_cast<size_t>(slots_) * slot_bytes; }
size_t   TraceLayout::slot(uint32_t index) const { return header_bytes + static_cast<size_t>(index) * slot_bytes; }

size_t TraceLayout::frame(uint32_t index, uint32_t depth) const
{
    return slot(index) + slot_header_bytes + static_cast<size_t>(depth) * frame_bytes;
}

}  // namespace av
