#include "harness/executor/d3d_loopback.hpp"

#include "device/abi/av_executor.h"
#include "executor/d3d/d3d_backend.hpp"
#include "executor/host_memory_source.hpp"
#include "executor/source_memory.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdlib>
#include <unistd.h>

namespace av {

namespace {

/* What the guest's kernel rounds every allocation to. */
constexpr std::uint64_t page_bytes = 4096u;

}  // namespace

D3dLoopback::D3dLoopback(D3dBackend &backend, HostMemorySource *source, std::size_t region_bytes,
                         std::size_t lent_bytes)
    : backend_(backend), source_(source), command_(static_cast<uint8_t *>(std::calloc(command_bytes, 1u))),
      reply_(static_cast<uint8_t *>(std::calloc(reply_bytes, 1u))),
      arena_block_(static_cast<uint8_t *>(std::aligned_alloc(16u, arena_bytes))), arena_(arena_block_, arena_bytes),
      decode_arena_(nullptr, 0u), blobs_(), refused_(0u), next_key_(1u), region_(), memories_()
{
    if (region_bytes == 0u) return;
    if (open(HostMemoryRequest(HostMemoryRequest::Kind::readback, 0u), region_bytes, region_))
        blobs_.lend(region_.memory().pointer(), lent_bytes);
}

D3dLoopback::~D3dLoopback()
{
    for (ImportedBlob &memory : memories_) close(memory);
    close(region_);
    std::free(command_);
    std::free(reply_);
    std::free(arena_block_);
}

/* As the device asks the executor for memory (HostMemorySource::allocate) and
 * the context then imports what it was given by key
 * (ContextSession::import_blob); the descriptor the device would map is not
 * needed here. */
bool D3dLoopback::open(HostMemoryRequest request, std::uint64_t bytes, ImportedBlob &blob)
{
    if (source_ == nullptr) return false;
    const std::uint64_t key = next_key_++;
    AvHostMemory        made = {-1, 0u, 0u};
    const std::uint64_t pages = (bytes + page_bytes - 1u) / page_bytes * page_bytes;
    if (source_->allocate(domain, key, pages, request, made) != Status::ok) return false;
    ::close(made.fd);
    SourceMemory *const memory = source_->acquire(domain, key);
    if (memory != nullptr && blob.take(key, *memory) == Status::ok && backend_.adopt(blob)) return true;
    if (blob.live()) static_cast<void>(blob.give_back());
    if (memory != nullptr) source_->release(*memory);
    source_->free(domain, key);
    return false;
}

void D3dLoopback::close(ImportedBlob &blob)
{
    if (!blob.live()) return;
    const std::uint64_t key = blob.id();
    backend_.abandon(blob);
    source_->release(*blob.give_back());
    source_->free(domain, key);
}

std::uint64_t D3dLoopback::open_memory(HostMemoryRequest request, std::uint64_t bytes)
{
    for (ImportedBlob &memory : memories_) {
        if (memory.live()) continue;
        return open(request, bytes, memory) ? memory.wire_handle() : 0u;
    }
    return 0u;
}

const SourceMemory *D3dLoopback::source_memory(std::uint64_t handle) const
{
    for (const ImportedBlob &memory : memories_)
        if (memory.live() && handle != 0u && memory.wire_handle() == handle) return &memory.memory();
    return nullptr;
}

std::uint8_t *D3dLoopback::memory(std::uint64_t handle, std::uint64_t offset) const
{
    const SourceMemory *const held = source_memory(handle);
    return held != nullptr && offset < held->size() ? held->pointer() + offset : nullptr;
}

void D3dLoopback::close_memory(std::uint64_t handle)
{
    for (ImportedBlob &memory : memories_)
        if (memory.live() && handle != 0u && memory.wire_handle() == handle) close(memory);
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

std::uint8_t *D3dLoopback::guest(std::size_t offset) const
{
    if (!region_.live() || offset >= region_.memory().size()) return nullptr;
    return region_.memory().pointer() + offset;
}

uint32_t D3dLoopback::refused() const
{
    return refused_;
}

}  // namespace av
