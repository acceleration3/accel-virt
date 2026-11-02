#include "core/trace/trace_text.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/scope_name.hpp"
#include "core/trace/trace_layout.hpp"
#include "core/trace/trace_slot_view.hpp"
#include "core/trace/trace_snapshot.hpp"

namespace av {

namespace {

constexpr uint64_t ticks_per_millisecond = TraceLayout::ticks_per_second / 1000u;
constexpr uint32_t address_digits = 16u;
constexpr uint32_t code_digits = 8u;
constexpr uint32_t id_digits = 8u;

}

TraceText::TraceText(const TraceSnapshot &snapshot, const ImageRange &image, uint64_t now)
    : snapshot_(snapshot), image_(image), now_(now), part_(Part::header), slot_(0u), frame_(0u)
{}

bool TraceText::next(TextBuffer &out)
{
    switch (part_) {
    case Part::header:
        header(out);
        part_ = snapshot_.crashed_at() != 0u ? Part::crash : Part::slot;
        if (part_ == Part::slot) advance_slot();
        return true;
    case Part::crash:
        crash(out);
        part_ = Part::slot;
        advance_slot();
        return true;
    case Part::slot:
        thread(out, snapshot_.slot(slot_));
        frame_ = 0u;
        part_ = Part::frame;
        break;
    case Part::frame:
        frame(out, snapshot_.slot(slot_));
        frame_++;
        break;
    case Part::more:
        out.append("  ... ");
        out.append_decimal(snapshot_.slot(slot_).depth() - snapshot_.slot(slot_).stored());
        out.append(" deeper frames not stored");
        slot_++;
        part_ = Part::slot;
        advance_slot();
        return true;
    default:
        return false;
    }
    const TraceSlotView view = snapshot_.slot(slot_);
    if (frame_ < view.stored()) return true;
    if (view.depth() > view.stored()) {
        part_ = Part::more;
        return true;
    }
    slot_++;
    part_ = Part::slot;
    advance_slot();
    return true;
}

bool TraceText::invariants_hold() const
{
    if (!image_.invariants_hold()) return false;
    if (part_ == Part::frame || part_ == Part::more) return slot_ < snapshot_.slots();
    return true;
}

void TraceText::header(TextBuffer &out) const
{
    out.append("trace module=");
    out.append(snapshot_.module());
    out.append(" pid=");
    out.append_decimal(snapshot_.process());
    out.append(" slots=");
    out.append_decimal(snapshot_.slots());
}

void TraceText::crash(TextBuffer &out) const
{
    out.append("crash code=0x");
    out.append_hex(snapshot_.crash_code(), code_digits);
    out.append(" address=0x");
    out.append_hex(snapshot_.crash_address(), address_digits);
    out.append(" image_offset=0x");
    out.append_hex(image_.offset_of(snapshot_.crash_address()), address_digits);
    out.append(" thread=");
    out.append_decimal(snapshot_.crash_thread());
}

void TraceText::thread(TextBuffer &out, const TraceSlotView &view) const
{
    out.append("thread ");
    out.append_decimal(view.thread_number());
    out.append(view.active() ? " active depth=" : " idle depth=");
    out.append_decimal(view.depth());
    out.append(" entries=");
    out.append_decimal(view.entries());
    if (view.waiting_on() != 0u) {
        out.append(" waiting=0x");
        out.append_hex(view.waiting_on(), address_digits);
    }
    if (view.holding() != 0u) {
        out.append(" holding=0x");
        out.append_hex(view.holding(), address_digits);
    }
    if (view.last_left() != 0u) {
        out.append(" last=#");
        out.append_hex(view.last_left(), id_digits);
        age(out, " left_ms=", view.left_at());
    }
}

void TraceText::frame(TextBuffer &out, const TraceSlotView &view) const
{
    out.append("  #");
    out.append_decimal(frame_);
    out.append(" ");
    name(out, view.name(frame_), view.text(frame_));
    out.append(" irql=");
    out.append_decimal(view.level(frame_));
    age(out, " for_ms=", view.entered(frame_));
}

void TraceText::name(TextBuffer &out, uint32_t id, uint64_t text) const
{
    char copy[max_name + 1u] = {};
    bool terminated = false;
    for (uint32_t length = 0u; length <= max_name && image_.contains(text + length, 1u); length++) {
        copy[length] = *reinterpret_cast<const char *>(static_cast<uintptr_t>(text + length));
        if (copy[length] != '\0') continue;
        terminated = length != 0u;
        break;
    }
    if (terminated && ScopeName(copy).id() == id) {
        out.append(copy);
        return;
    }
    out.append("#");
    out.append_hex(id, id_digits);
}

void TraceText::age(TextBuffer &out, const char *key, uint64_t since) const
{
    out.append(key);
    out.append_decimal(now_ >= since ? (now_ - since) / ticks_per_millisecond : 0u);
}

void TraceText::advance_slot()
{
    while (slot_ < snapshot_.slots() && !snapshot_.slot(slot_).used()) slot_++;
    if (slot_ >= snapshot_.slots()) part_ = Part::done;
}

}
