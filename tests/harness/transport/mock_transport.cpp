#include "harness/transport/mock_transport.hpp"

#include <cstring>

namespace av {

MockTransport::MockTransport()
    : request_{}, sent_{}, reply_{}, reply_length_(0u), sends_(0u), end_calls_(0u), last_command_(0u),
      last_length_(0u), fail_next_(false), arena_(nullptr, 0u), blobs_() {}

void MockTransport::set_reply(const std::uint8_t *reply, std::size_t length)
{
    reply_length_ = length < slot_bytes ? length : slot_bytes;
    std::memcpy(reply_, reply, reply_length_);
}

void          MockTransport::fail_next() { fail_next_ = true; }
std::uint32_t MockTransport::sends() const { return sends_; }
std::uint32_t MockTransport::end_calls() const { return end_calls_; }
void          MockTransport::end_call() { end_calls_++; }
std::uint32_t MockTransport::last_command() const { return last_command_; }
std::size_t   MockTransport::last_length() const { return last_length_; }
const std::uint8_t *MockTransport::last_payload() const { return sent_; }

WireWriter MockTransport::request_writer() { return WireWriter(request_, slot_bytes); }

TransportResult MockTransport::deliver(std::uint32_t command_id, WireWriter &writer)
{
    if (fail_next_) {
        fail_next_ = false;
        return TransportResult::failed;
    }
    if (writer.status() != Status::ok) return TransportResult::failed;
    last_command_ = command_id;
    last_length_ = writer.offset();
    std::memcpy(sent_, request_, last_length_);
    sends_++;
    return TransportResult::ok;
}

TransportResult MockTransport::send_async(std::uint32_t command_id, WireWriter &writer)
{
    return deliver(command_id, writer);
}

TransportResult MockTransport::send_sync(std::uint32_t command_id, WireWriter &writer,
                                         WireReader &reply)
{
    const TransportResult result = deliver(command_id, writer);
    if (result != TransportResult::ok) return result;
    reply = WireReader(reply_, reply_length_);
    return TransportResult::ok;
}

WireArena       &MockTransport::decode_arena() { return arena_; }
ClientBlobArena &MockTransport::blobs() { return blobs_; }

}
