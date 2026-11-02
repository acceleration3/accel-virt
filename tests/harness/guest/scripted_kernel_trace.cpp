#include "harness/guest/scripted_kernel_trace.hpp"

#include <cstring>

namespace av {

namespace {

constexpr std::uint32_t status_invalid_parameter = 0xC000000Du;
constexpr std::uint32_t status_unsuccessful = 0xC0000001u;

}

ScriptedKernelTrace::ScriptedKernelTrace()
    : table_(nullptr), table_length_(0u), state_(nullptr), state_length_(0u), claimed_(0u), claiming_(false),
      now_(0u), answers_(0u), fault_at_(0u), fault_(Fault::none)
{}

std::uint32_t ScriptedKernelTrace::answer(std::uint8_t *data, std::uint32_t size)
{
    TraceEscapeRecord request;
    if (request.decode(data, size) != Status::ok) return status_invalid_parameter;
    answers_++;
    const Fault fault = answers_ == fault_at_ ? fault_ : Fault::none;
    if (fault == Fault::refused) return status_unsuccessful;
    const bool                table = request.kind() == TraceEscapeRecord::Kind::table;
    const std::uint8_t *const bytes = table ? table_ : state_;
    const std::size_t         length = table ? table_length_ : state_length_;
    std::uint64_t             total = claiming_ ? claimed_ : length;
    if (fault == Fault::grown_total) total++;
    const std::uint64_t left = request.offset() < total ? total - request.offset() : 0u;
    std::uint32_t       filled = left < request.window() ? static_cast<std::uint32_t>(left) : request.window();
    if (fault == Fault::short_window && filled != 0u) filled--;
    if (fault == Fault::zero_window) filled = 0u;
    for (std::uint32_t byte = 0u; byte < filled; byte++) {
        const std::uint64_t at = request.offset() + byte;
        data[TraceEscapeRecord::encoded_size + byte] = at < length ? bytes[at] : 0u;
    }
    TraceEscapeRecord answered = request.answered(total, filled, now_);
    if (fault == Fault::other_offset)
        answered = TraceEscapeRecord(request.kind(), request.offset() + 1u, request.window()).answered(total, 0u, now_);
    static_cast<void>(answered.encode(data, size));
    return 0u;
}

void ScriptedKernelTrace::serve(TraceEscapeRecord::Kind kind, const std::uint8_t *bytes, std::size_t length)
{
    if (kind == TraceEscapeRecord::Kind::table) {
        table_ = bytes;
        table_length_ = length;
        return;
    }
    state_ = bytes;
    state_length_ = length;
}

void ScriptedKernelTrace::claim_total(std::uint64_t total)
{
    claimed_ = total;
    claiming_ = true;
}

void ScriptedKernelTrace::set_clock(std::uint64_t now) { now_ = now; }

void ScriptedKernelTrace::fault_at(std::uint32_t nth, Fault fault)
{
    fault_at_ = answers_ + nth;
    fault_ = fault;
}

std::uint32_t ScriptedKernelTrace::answers() const { return answers_; }

}
