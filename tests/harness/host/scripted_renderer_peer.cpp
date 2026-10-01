#include "harness/host/scripted_renderer_peer.hpp"

#include "device/abi/av_executor.h"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/renderer/reply_message.hpp"

#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace av {

namespace {

constexpr std::uint32_t no_import_kinds[ReadyMessage::lanes] = {};

}  // namespace

ScriptedRendererPeer::ScriptedRendererPeer(MockHost &host)
    : host_(host), datagrams_{}, delivered_(0u), answers_{}, first_(0u), queued_(0u), outcome_(Status::ok),
      ready_(1u, AV_EXECUTOR_ABI_VERSION, AV_EXECUTOR_SUB_API_VULKAN, no_import_kinds), silent_(false),
      dying_(false)
{
}

ScriptedRendererPeer::~ScriptedRendererPeer() = default;

void ScriptedRendererPeer::deliver(const std::uint8_t *bytes, std::size_t length, const int *descriptors,
                                   std::uint32_t count)
{
    Datagram entry = {};
    for (std::uint32_t index = 0u; index < count; index++) {
        struct stat file = {};
        if (descriptors[index] >= 0 && ::fstat(descriptors[index], &file) == 0) entry.open++;
        if (descriptors[index] >= 0) ::close(descriptors[index]);
    }
    entry.descriptors = count;
    RendererMessageHeader header;
    const bool            framed = header.decode(bytes, length) == Status::ok;
    if (framed) {
        entry.kind = header.kind();
        entry.sequence = header.sequence();
        entry.body_length = header.body_length() < max_body ? header.body_length() : static_cast<std::uint32_t>(max_body);
        std::memcpy(entry.body, bytes + RendererMessageHeader::encoded_size, entry.body_length);
    }
    if (delivered_ < max_datagrams) datagrams_[delivered_] = entry;
    else TestRegistry::instance().record_failure(__FILE__, __LINE__, "more datagrams than the peer keeps");
    delivered_++;
    if (dying_) {
        dying_ = false;
        host_.renderer_gone();
        return;
    }
    if (framed && !silent_) answer_request(header);
}

void ScriptedRendererPeer::answer_request(const RendererMessageHeader &header)
{
    using Kind = RendererMessageHeader::Kind;
    std::uint8_t bytes[RendererMessageHeader::encoded_size + max_body] = {};
    if (header.kind() == Kind::hello) {
        const RendererMessageHeader out(Kind::ready, ReadyMessage::encoded_size, header.sequence());
        if (out.encode(bytes, sizeof bytes) != Status::ok ||
            ready_.encode(bytes + RendererMessageHeader::encoded_size, sizeof bytes - RendererMessageHeader::encoded_size) !=
                Status::ok)
            TestRegistry::instance().record_failure(__FILE__, __LINE__, "the scripted Ready does not encode");
        push(bytes, RendererMessageHeader::encoded_size + ReadyMessage::encoded_size, 0u);
        return;
    }
    if (header.kind() == Kind::create_context || header.kind() == Kind::destroy_context ||
        header.kind() == Kind::bind_context)
        queue_reply(header.sequence(), header.sequence(), outcome_);
}

bool ScriptedRendererPeer::answer(std::uint8_t *buffer, std::size_t capacity, std::size_t &length, int *descriptors,
                                  std::uint32_t capacity_descriptors, std::uint32_t &count)
{
    count = 0u;
    if (queued_ == 0u) return false;
    const Answer &next = answers_[first_];
    first_ = (first_ + 1u) % max_queued;
    queued_--;
    length = next.length < capacity ? next.length : capacity;
    std::memcpy(buffer, next.bytes, length);
    for (std::uint32_t index = 0u; index < next.descriptors && index < capacity_descriptors; index++) {
        int ends[2] = {-1, -1};
        if (::pipe2(ends, O_CLOEXEC) != 0) break;
        ::close(ends[1]);
        descriptors[count++] = ends[0];
    }
    return true;
}

void ScriptedRendererPeer::push(const std::uint8_t *bytes, std::size_t length, std::uint32_t descriptors)
{
    if (queued_ == max_queued || length > sizeof answers_[0].bytes) {
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the scripted answer does not fit");
        return;
    }
    Answer &slot = answers_[(first_ + queued_) % max_queued];
    std::memcpy(slot.bytes, bytes, length);
    slot.length = length;
    slot.descriptors = descriptors < max_attached ? descriptors : max_attached;
    queued_++;
}

void ScriptedRendererPeer::queue(const std::uint8_t *bytes, std::size_t length, std::uint32_t descriptors)
{
    push(bytes, length, descriptors);
}

void ScriptedRendererPeer::queue_reply(std::uint64_t sequence, Status outcome, std::uint32_t descriptors)
{
    const ReplyMessage          reply(sequence, outcome, nullptr, 0u);
    const RendererMessageHeader header(RendererMessageHeader::Kind::reply,
                                       static_cast<std::uint32_t>(reply.encoded_length()), sequence);
    std::uint8_t                bytes[RendererMessageHeader::encoded_size + max_body] = {};
    if (header.encode(bytes, sizeof bytes) != Status::ok ||
        reply.encode(bytes + RendererMessageHeader::encoded_size, sizeof bytes - RendererMessageHeader::encoded_size) !=
            Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the scripted reply does not encode");
    push(bytes, RendererMessageHeader::encoded_size + reply.encoded_length(), descriptors);
}

void ScriptedRendererPeer::queue_reply(std::uint64_t header_sequence, std::uint64_t body_sequence, Status outcome)
{
    const ReplyMessage          reply(body_sequence, outcome, nullptr, 0u);
    const RendererMessageHeader header(RendererMessageHeader::Kind::reply,
                                       static_cast<std::uint32_t>(reply.encoded_length()), header_sequence);
    std::uint8_t                bytes[RendererMessageHeader::encoded_size + max_body] = {};
    if (header.encode(bytes, sizeof bytes) != Status::ok ||
        reply.encode(bytes + RendererMessageHeader::encoded_size, sizeof bytes - RendererMessageHeader::encoded_size) !=
            Status::ok)
        TestRegistry::instance().record_failure(__FILE__, __LINE__, "the scripted reply does not encode");
    push(bytes, RendererMessageHeader::encoded_size + reply.encoded_length(), 0u);
}

void ScriptedRendererPeer::answer_requests_with(Status outcome) { outcome_ = outcome; }
void ScriptedRendererPeer::ready_with(const ReadyMessage &ready) { ready_ = ready; }
void ScriptedRendererPeer::fall_silent() { silent_ = true; }
void ScriptedRendererPeer::die_on_next_delivery() { dying_ = true; }
std::uint32_t ScriptedRendererPeer::delivered() const { return delivered_; }
std::uint32_t ScriptedRendererPeer::queued() const { return queued_; }

RendererMessageHeader::Kind ScriptedRendererPeer::kind_at(std::uint32_t index) const
{
    return index < max_datagrams ? datagrams_[index].kind : RendererMessageHeader::Kind{};
}

std::uint64_t ScriptedRendererPeer::sequence_at(std::uint32_t index) const
{
    return index < max_datagrams ? datagrams_[index].sequence : 0u;
}

std::uint32_t ScriptedRendererPeer::descriptors_at(std::uint32_t index) const
{
    return index < max_datagrams ? datagrams_[index].descriptors : 0u;
}

std::uint32_t ScriptedRendererPeer::open_descriptors_at(std::uint32_t index) const
{
    return index < max_datagrams ? datagrams_[index].open : 0u;
}

const std::uint8_t *ScriptedRendererPeer::body_at(std::uint32_t index) const
{
    return index < max_datagrams ? datagrams_[index].body : nullptr;
}

std::uint32_t ScriptedRendererPeer::body_length_at(std::uint32_t index) const
{
    return index < max_datagrams ? datagrams_[index].body_length : 0u;
}

}  // namespace av
