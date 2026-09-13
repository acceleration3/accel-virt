#include "harness/executor/mock_session_backend.hpp"

#include "device/executor/imported_blob.hpp"
#include "proto/wire/wire_arena.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

MockSessionBackend::MockSessionBackend()
    : answer_bytes_(0u), ran_(0u), adopted_(0u), abandoned_(0u), lost_(false), adopts_(true)
{}

void MockSessionBackend::set_answer_bytes(std::size_t bytes) { answer_bytes_ = bytes; }

std::uint32_t MockSessionBackend::ran() const { return ran_; }

bool MockSessionBackend::knows(std::uint32_t command_id) const
{
    return command_id == echo_id || command_id == long_answer_id || command_id == malformed_id ||
           command_id == greedy_id || command_id == lose_id;
}

bool MockSessionBackend::run(std::uint32_t command_id, WireReader &payload, WireWriter &reply,
                             WireArena &arena)
{
    ran_++;
    if (command_id == malformed_id) return false;
    if (command_id == greedy_id) {
        /* Whatever the block is, this asks for more than it. */
        return arena.allocate<std::uint8_t>(arena.capacity() + 1u) != nullptr;
    }
    if (command_id == long_answer_id) {
        reply.write_zeros(answer_bytes_);
        return true;
    }
    if (command_id == lose_id) {
        lost_ = true;
        return true;
    }
    /* echo_id: the payload is the answer. */
    while (payload.remaining() != 0u) {
        std::uint8_t byte = 0u;
        if (!payload.read_pod(byte)) return false;
        reply.write_pod(byte);
    }
    return true;
}

bool MockSessionBackend::lost() const { return lost_; }

void          MockSessionBackend::set_adopts(bool adopts) { adopts_ = adopts; }
std::uint32_t MockSessionBackend::adopted() const { return adopted_; }
std::uint32_t MockSessionBackend::abandoned() const { return abandoned_; }

bool MockSessionBackend::adopt(ImportedBlob &blob)
{
    if (!adopts_) return false;
    adopted_++;
    /* Whatever this API holds memory as; a number is all the session keeps. */
    blob.set_object(blob.id() + 0x1000u);
    return true;
}

void MockSessionBackend::abandon(ImportedBlob &blob)
{
    abandoned_++;
    blob.set_object(0u);
}

}  // namespace av
