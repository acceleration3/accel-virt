#include "harness/loopback/loopback_driver.hpp"

#include "core/av/capset_query.hpp"
#include "core/av/guest_context.hpp"
#include "harness/loopback/loopback_device.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

namespace av {

LoopbackDriver::LoopbackDriver(MockPlatform &platform, LoopbackDevice &device)
    : platform_(platform), device_(device), tamper_at_(0u), tamper_offset_(0u), tamper_value_(0u),
      truncate_at_(0u), truncate_length_(0u) {}

std::size_t LoopbackDriver::exchange(const std::uint8_t *request, std::size_t length, std::uint8_t *answer)
{
    std::size_t answered = device_.exchange(request, length, answer, answer_capacity);
    if (tamper_at_ != 0u && --tamper_at_ == 0u && tamper_offset_ < answered) answer[tamper_offset_] = tamper_value_;
    if (truncate_at_ != 0u && --truncate_at_ == 0u && truncate_length_ < answered) answered = truncate_length_;
    return answered;
}

Status LoopbackDriver::query(CapsetQuery &query)
{
    Status       accepted = Status::ok;
    std::uint8_t request[AvRequestWriter::max_request_size] = {};
    std::size_t  length = 0u;
    platform_.set_irql(Irql::dispatch);
    while (query.next_request(request, sizeof request, length) == Status::ok) {
        std::uint8_t      answer[answer_capacity] = {};
        const std::size_t answered = exchange(request, length, answer);
        accepted = query.accept(answer, answered);
    }
    platform_.set_irql(Irql::passive);
    return accepted;
}

std::uint32_t LoopbackDriver::converse(GuestContext &context)
{
    std::uint32_t exchanged = 0u;
    std::uint8_t  request[AvRequestWriter::max_request_size] = {};
    std::size_t   length = 0u;
    platform_.set_irql(Irql::dispatch);
    while (context.next_request(request, sizeof request, length) == Status::ok) {
        std::uint8_t      answer[answer_capacity] = {};
        const std::size_t answered = exchange(request, length, answer);
        static_cast<void>(context.accept(answer, answered));
        exchanged++;
    }
    platform_.set_irql(Irql::passive);
    return exchanged;
}

bool LoopbackDriver::serve_until(GuestContext &context, std::uint32_t line, std::uint64_t fence)
{
    for (;;) {
        const std::uint32_t seen = device_.host().raised(line);
        platform_.set_irql(Irql::dispatch);
        const Status served = context.serve();
        platform_.set_irql(Irql::passive);
        if (context.completed() >= fence) return true;
        if (served != Status::ok || context.faulted() || !device_.raised_past(line, seen)) return false;
    }
}

void LoopbackDriver::tamper_answer(std::uint32_t nth, std::size_t offset, std::uint8_t value)
{
    tamper_at_ = nth;
    tamper_offset_ = offset;
    tamper_value_ = value;
}

void LoopbackDriver::truncate_answer(std::uint32_t nth, std::size_t length)
{
    truncate_at_ = nth;
    truncate_length_ = length;
}

}  // namespace av
