#include "harness/executor/d3d_loopback.hpp"

#include "device/abi/av_executor.h"
#include "executor/d3d/d3d_backend.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdlib>

namespace av {

D3dLoopback::D3dLoopback(D3dBackend &backend, std::size_t guest_bytes, std::size_t lent_bytes)
    : backend_(backend), command_(static_cast<uint8_t *>(std::calloc(command_bytes, 1u))),
      reply_(static_cast<uint8_t *>(std::calloc(reply_bytes, 1u))),
      arena_block_(static_cast<uint8_t *>(std::aligned_alloc(16u, arena_bytes))), arena_(arena_block_, arena_bytes),
      decode_arena_(nullptr, 0u), blobs_(), refused_(0u), guest_(nullptr), guest_bytes_(0u), blob_()
{
    constexpr std::size_t heap_alignment = 64u * 1024u;
    if (guest_bytes == 0u) return;
    guest_ = static_cast<uint8_t *>(std::aligned_alloc(heap_alignment, guest_bytes));
    guest_bytes_ = guest_bytes;
    AvBlobHandle handle = {};
    handle.kind = AV_BLOB_HOST_POINTER;
    handle.pointer = guest_;
    handle.fd = -1;
    handle.size = guest_bytes;
    if (blob_.take(1u, handle) == Status::ok && backend_.adopt(blob_)) blobs_.lend(guest_, lent_bytes);
}

D3dLoopback::~D3dLoopback()
{
    if (blob_.live()) backend_.abandon(blob_);
    std::free(guest_);
    std::free(command_);
    std::free(reply_);
    std::free(arena_block_);
}

WireWriter D3dLoopback::request_writer()
{
    return WireWriter(command_, command_bytes);
}

bool D3dLoopback::run(uint32_t command_id, WireWriter &writer, bool awaited, std::size_t &answered)
{
    answered = 0u;
    if (writer.status() != Status::ok) return false;
    WireReader payload(command_, writer.offset());
    /* Only an answer the guest waits on is lent room, as the device's batch
     * executor lends it: that is how a session tells the two apart. */
    WireWriter reply(awaited ? reply_ : nullptr, awaited ? reply_bytes : 0u);
    arena_.reset();
    /* Each command is a batch of its own here, ended as the worker ends one:
     * whether it ran or was refused, the Direct3D 11 lock it took is let go. */
    const bool ran = backend_.run(command_id, payload, reply, arena_);
    backend_.end_batch();
    if (!ran) {
        refused_++;
        return false;
    }
    answered = reply.offset();
    return true;
}

TransportResult D3dLoopback::send_async(uint32_t command_id, WireWriter &writer)
{
    std::size_t answered = 0u;
    return run(command_id, writer, false, answered) ? TransportResult::ok : TransportResult::failed;
}

TransportResult D3dLoopback::send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply)
{
    std::size_t answered = 0u;
    if (!run(command_id, writer, true, answered)) return TransportResult::failed;
    /* An answer means the host has read every lease sent before it. */
    blobs_.retire_all();
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

uint8_t *D3dLoopback::guest(std::size_t offset) const
{
    return guest_ != nullptr && offset < guest_bytes_ ? guest_ + offset : nullptr;
}

uint32_t D3dLoopback::refused() const
{
    return refused_;
}

}  // namespace av
