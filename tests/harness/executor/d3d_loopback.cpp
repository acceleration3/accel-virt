#include "harness/executor/d3d_loopback.hpp"

#include "device/executor/d3d_backend.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdlib>

namespace av {

D3dLoopback::D3dLoopback(D3dBackend &backend)
    : backend_(backend), command_(static_cast<uint8_t *>(std::calloc(command_bytes, 1u))),
      reply_(static_cast<uint8_t *>(std::calloc(reply_bytes, 1u))),
      arena_block_(static_cast<uint8_t *>(std::aligned_alloc(16u, arena_bytes))), arena_(arena_block_, arena_bytes),
      decode_arena_(nullptr, 0u), blobs_(), refused_(0u)
{
}

D3dLoopback::~D3dLoopback()
{
    std::free(command_);
    std::free(reply_);
    std::free(arena_block_);
}

WireWriter D3dLoopback::request_writer()
{
    return WireWriter(command_, command_bytes);
}

bool D3dLoopback::run(uint32_t command_id, WireWriter &writer, std::size_t &answered)
{
    answered = 0u;
    if (writer.status() != Status::ok) return false;
    WireReader payload(command_, writer.offset());
    WireWriter reply(reply_, reply_bytes);
    arena_.reset();
    if (!backend_.run(command_id, payload, reply, arena_)) {
        refused_++;
        return false;
    }
    answered = reply.offset();
    return true;
}

TransportResult D3dLoopback::send_async(uint32_t command_id, WireWriter &writer)
{
    std::size_t answered = 0u;
    return run(command_id, writer, answered) ? TransportResult::ok : TransportResult::failed;
}

TransportResult D3dLoopback::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    std::size_t answered = 0u;
    if (!run(command_id, writer, answered)) return TransportResult::failed;
    reply = WireReader(reply_, answered);
    return TransportResult::ok;
}

void D3dLoopback::end_call()
{
}

WireArena &D3dLoopback::decode_arena()
{
    return decode_arena_;
}

ClientBlobArena &D3dLoopback::blobs()
{
    return blobs_;
}

uint32_t D3dLoopback::refused() const
{
    return refused_;
}

}  // namespace av
