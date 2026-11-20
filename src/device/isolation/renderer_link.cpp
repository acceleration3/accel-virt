#include "device/isolation/renderer_link.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/renderer/ready_message.hpp"
#include "proto/renderer/reply_message.hpp"
#include "core/support/text_buffer.hpp"

namespace av {

RendererLink::RendererLink(Host &host) : host_(host), sequence_(0u), framed_(0u), datagram_{}
{
    AV_SCOPE("RendererLink::RendererLink");
}

Status RendererLink::frame(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                           uint64_t &sequence)
{
    AV_SCOPE("RendererLink::frame");
    if (length > RendererMessageHeader::max_body_length || (length != 0u && body == nullptr))
        return Status::invalid_parameter;
    sequence = ++sequence_;
    const RendererMessageHeader header(kind, length, sequence);
    const Status                encoded = header.encode(datagram_, sizeof datagram_);
    if (encoded != Status::ok) return encoded;
    for (uint32_t index = 0u; index < length; index++) datagram_[RendererMessageHeader::encoded_size + index] = body[index];
    framed_ = RendererMessageHeader::encoded_size + length;
    return Status::ok;
}

Status RendererLink::post(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                          const int *descriptors, uint32_t count)
{
    AV_SCOPE("RendererLink::post");
    host_.require_main_loop("RendererLink::post");
    uint64_t     sequence = 0u;
    const Status framed = frame(kind, body, length, sequence);
    if (framed != Status::ok) return framed;
    return host_.send_renderer(datagram_, framed_, descriptors, count);
}

Status RendererLink::request(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                             const int *descriptors, uint32_t count, Status &outcome)
{
    AV_SCOPE("RendererLink::request");
    host_.require_main_loop("RendererLink::request");
    outcome = Status::invalid_state;
    uint64_t sequence = 0u;
    Status   status = frame(kind, body, length, sequence);
    if (status == Status::ok) status = host_.send_renderer(datagram_, framed_, descriptors, count);
    if (status != Status::ok) return status;
    const uint8_t *answer = nullptr;
    uint32_t       answer_length = 0u;
    uint32_t       kept = 0u;
    status = await(kind, RendererMessageHeader::Kind::reply, sequence, request_timeout_ms, answer, answer_length, nullptr, 0u,
                   kept);
    if (status != Status::ok) return status;
    ReplyMessage reply;
    if (reply.decode(answer, answer_length, 0u) != Status::ok || reply.sequence() != sequence) return Status::malformed;
    outcome = reply.status();
    return Status::ok;
}

Status RendererLink::greet(const uint8_t *body, uint32_t length, ReadyMessage &ready)
{
    AV_SCOPE("RendererLink::greet");
    host_.require_main_loop("RendererLink::greet");
    uint64_t sequence = 0u;
    Status   status = frame(RendererMessageHeader::Kind::hello, body, length, sequence);
    if (status == Status::ok) status = host_.send_renderer(datagram_, framed_, nullptr, 0u);
    if (status != Status::ok) return status;
    const uint8_t *answer = nullptr;
    uint32_t       answer_length = 0u;
    uint32_t       kept = 0u;
    status = await(RendererMessageHeader::Kind::hello, RendererMessageHeader::Kind::ready, sequence, hello_timeout_ms, answer, answer_length, nullptr, 0u,
                   kept);
    if (status != Status::ok) return status;
    return ready.decode(answer, answer_length, 0u) == Status::ok ? Status::ok : Status::malformed;
}

Status RendererLink::exchange(RendererMessageHeader::Kind kind, const uint8_t *body, uint32_t length,
                              RendererMessageHeader::Kind answered, const uint8_t *&answer, uint32_t &answer_length,
                              int *descriptors, uint32_t capacity, uint32_t &count)
{
    AV_SCOPE("RendererLink::exchange");
    host_.require_main_loop("RendererLink::exchange");
    count = 0u;
    uint64_t sequence = 0u;
    Status   status = frame(kind, body, length, sequence);
    if (status == Status::ok) status = host_.send_renderer(datagram_, framed_, nullptr, 0u);
    if (status != Status::ok) return status;
    return await(kind, answered, sequence, request_timeout_ms, answer, answer_length, descriptors, capacity, count);
}

void RendererLink::extend(bool &extended, uint64_t &deadline, uint64_t started, RendererMessageHeader::Kind asked,
                          uint32_t timeout_ms)
{
    AV_SCOPE("RendererLink::extend");
    constexpr uint64_t nanoseconds_per_millisecond = 1000000u;
    extended = true;
    deadline = started + uint64_t{slow_timeout_ms} * nanoseconds_per_millisecond;
    char       line[128] = {};
    TextBuffer message(line, sizeof line);
    message.append("virtio-gpu-av: the renderer has not answered ");
    switch (asked) {
    case RendererMessageHeader::Kind::hello: message.append("hello"); break;
    case RendererMessageHeader::Kind::ready: message.append("ready"); break;
    case RendererMessageHeader::Kind::region: message.append("region"); break;
    case RendererMessageHeader::Kind::guest_blob: message.append("guest_blob"); break;
    case RendererMessageHeader::Kind::allocate_memory: message.append("allocate_memory"); break;
    case RendererMessageHeader::Kind::blob_retired: message.append("blob_retired"); break;
    case RendererMessageHeader::Kind::create_context: message.append("create_context"); break;
    case RendererMessageHeader::Kind::bind_context: message.append("bind_context"); break;
    case RendererMessageHeader::Kind::destroy_context: message.append("destroy_context"); break;
    case RendererMessageHeader::Kind::interrupt_lines: message.append("interrupt_lines"); break;
    case RendererMessageHeader::Kind::interrupts_off: message.append("interrupts_off"); break;
    case RendererMessageHeader::Kind::reply: message.append("reply"); break;
    case RendererMessageHeader::Kind::export_image: message.append("export_image"); break;
    case RendererMessageHeader::Kind::exported_image: message.append("exported_image"); break;
    case RendererMessageHeader::Kind::shared_released: message.append("shared_released"); break;
    case RendererMessageHeader::Kind::allocated_memory: message.append("allocated_memory"); break;
    case RendererMessageHeader::Kind::free_memory: message.append("free_memory"); break;
    }
    message.append(" in ");
    message.append_decimal(timeout_ms);
    message.append(" ms; waiting up to ");
    message.append_decimal(slow_timeout_ms);
    message.append(" ms");
    host_.log(message.text());
}

Status RendererLink::await(RendererMessageHeader::Kind asked, RendererMessageHeader::Kind expected, uint64_t sequence, uint32_t timeout_ms,
                           const uint8_t *&body, uint32_t &length, int *kept, uint32_t capacity, uint32_t &count)
{
    AV_SCOPE("RendererLink::await");
    count = 0u;
    constexpr uint64_t nanoseconds_per_millisecond = 1000000u;
    const uint64_t     started = host_.monotonic_nanoseconds();
    uint64_t           deadline = started + uint64_t{timeout_ms} * nanoseconds_per_millisecond;
    bool               extended = timeout_ms >= slow_timeout_ms;
    for (;;) {
        const uint64_t now = host_.monotonic_nanoseconds();
        if (now >= deadline) {
            if (extended) return Status::invalid_state;
            extend(extended, deadline, started, asked, timeout_ms);
            continue;
        }
        const uint32_t left = static_cast<uint32_t>((deadline - now + nanoseconds_per_millisecond - 1u) /
                                                    nanoseconds_per_millisecond);
        int          descriptors[max_descriptors] = {};
        uint32_t     arrived = 0u;
        size_t       received = 0u;
        const Status status =
            host_.receive_renderer(datagram_, sizeof datagram_, received, descriptors, max_descriptors, arrived, left);
        if (status == Status::invalid_state && !extended) {
            extend(extended, deadline, started, asked, timeout_ms);
            continue;
        }
        if (status != Status::ok) return status;
        RendererMessageHeader header;
        const bool            awaited = header.decode(datagram_, received) == Status::ok && header.kind() == expected &&
                             header.sequence() == sequence;
        for (uint32_t index = 0u; index < arrived; index++) {
            if (awaited && kept != nullptr && count < capacity) kept[count++] = descriptors[index];
            else host_.close_descriptor(descriptors[index]);
        }
        if (!awaited) continue;
        body = datagram_ + RendererMessageHeader::encoded_size;
        length = header.body_length();
        return Status::ok;
    }
}

}
