#include "um/d3d/flip_shown_wait.hpp"

#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "um/d3d/d3d_session.hpp"

#include <windows.h>

namespace av::d3d {

namespace {
constexpr uint64_t bound_ms = 40u;
constexpr uint64_t past_a_refresh_ms = 21u;
constexpr uint64_t settle_ms = 4u;
constexpr uint64_t ms_per_second = 1000u;

}

FlipShownWait::FlipShownWait(D3dSession &session, KeyedLog &lines)
    : session_(session), lines_(lines), source_(0u), pending_(false), flipped_(), flipped_at_(0u), frequency_(0u),
      refused_()
{
    AV_SCOPE("FlipShownWait::FlipShownWait");
    LARGE_INTEGER frequency = {};
    if (QueryPerformanceFrequency(&frequency) && frequency.QuadPart > 0)
        frequency_ = static_cast<uint64_t>(frequency.QuadPart);
}

uint64_t FlipShownWait::milliseconds() const
{
    AV_SCOPE("FlipShownWait::milliseconds");
    LARGE_INTEGER now = {};
    if (frequency_ == 0u || !QueryPerformanceCounter(&now)) return 0u;
    return static_cast<uint64_t>(now.QuadPart) * ms_per_second / frequency_;
}

void FlipShownWait::flipped(uint32_t source)
{
    AV_SCOPE("FlipShownWait::flipped");
    source_ = source;
    pending_ = session_.scan_line(source, flipped_) == Status::ok;
    flipped_at_ = milliseconds();
}

void FlipShownWait::await()
{
    AV_SCOPE("FlipShownWait::await");
    if (!pending_) return;
    pending_ = false;
    const Status blank = await_blank();
    if (blank == Status::ok || !refused_.first()) return;
    LogLine(lines_)
        .text("a primary was written without a blank of source ")
        .number(source_)
        .text(" after the flip before it, status ")
        .number(static_cast<uint32_t>(blank))
        .write();
}

Status FlipShownWait::await_blank()
{
    AV_SCOPE("FlipShownWait::await_blank");
    uint64_t blank_seen_at = 0u;
    for (;;) {
        const uint64_t now_ms = milliseconds();
        if (now_ms - flipped_at_ >= past_a_refresh_ms) return Status::ok;
        if (blank_seen_at != 0u && now_ms - blank_seen_at >= settle_ms) return Status::ok;
        if (now_ms - flipped_at_ >= bound_ms) return Status::not_found;
        if (blank_seen_at == 0u) {
            BeamPosition now;
            const Status read = session_.scan_line(source_, now);
            if (read != Status::ok) return read;
            if (now.earlier_than(flipped_)) blank_seen_at = now_ms;
        }
        Sleep(1u);
    }
}

}
