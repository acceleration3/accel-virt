#include "core/trace/trace_slot_view.hpp"

#include "core/trace/trace_layout.hpp"

namespace av {

TraceSlotView::TraceSlotView() : slot_(nullptr), frames_(0u) {}
TraceSlotView::TraceSlotView(const uint8_t *slot, uint32_t frames) : slot_(slot), frames_(frames) {}

bool     TraceSlotView::used() const { return read(TraceLayout::owner_at, 8u) != 0u; }
bool     TraceSlotView::active() const { return (read(TraceLayout::owner_at, 8u) & TraceLayout::active) != 0u; }
uint64_t TraceSlotView::thread_key() const { return read(TraceLayout::owner_at, 8u) & ~TraceLayout::active; }
uint32_t TraceSlotView::thread_number() const { return static_cast<uint32_t>(read(TraceLayout::thread_at, 4u)); }
uint32_t TraceSlotView::depth() const { return static_cast<uint32_t>(read(TraceLayout::depth_at, 4u)); }
uint32_t TraceSlotView::stored() const { return depth() < frames_ ? depth() : frames_; }
uint64_t TraceSlotView::entries() const { return read(TraceLayout::entries_at, 8u); }
uint32_t TraceSlotView::last_left() const { return static_cast<uint32_t>(read(TraceLayout::last_left_at, 4u)); }
uint64_t TraceSlotView::left_at() const { return read(TraceLayout::left_at, 8u); }
uint64_t TraceSlotView::waiting_on() const { return read(TraceLayout::waiting_at, 8u); }
uint64_t TraceSlotView::holding() const { return read(TraceLayout::holding_at, 8u); }

uint32_t TraceSlotView::name(uint32_t frame) const
{
    return static_cast<uint32_t>(frame_field(frame, TraceLayout::name_at, 4u));
}

uint16_t TraceSlotView::level(uint32_t frame) const
{
    return static_cast<uint16_t>(frame_field(frame, TraceLayout::level_at, 2u));
}

uint64_t TraceSlotView::entered(uint32_t frame) const { return frame_field(frame, TraceLayout::entered_at, 8u); }
uint64_t TraceSlotView::text(uint32_t frame) const { return frame_field(frame, TraceLayout::text_at, 8u); }

uint64_t TraceSlotView::read(uint32_t offset, uint32_t bytes) const
{
    if (slot_ == nullptr) return 0u;
    uint64_t value = 0u;
    for (uint32_t byte = 0u; byte < bytes; byte++) value |= static_cast<uint64_t>(slot_[offset + byte]) << (8u * byte);
    return value;
}

uint64_t TraceSlotView::frame_field(uint32_t frame, uint32_t offset, uint32_t bytes) const
{
    if (frame >= stored()) return 0u;
    return read(TraceLayout::slot_header_bytes + frame * TraceLayout::frame_bytes + offset, bytes);
}

}  // namespace av
