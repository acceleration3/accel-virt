/* One guest context, running on one host API.
 *
 * What the device holds as an opaque `AvExecutor *`. It owns the block one
 * command decodes into and, later, the blobs that context imported; it does
 * not own the backend, which outlives a single session no more than it
 * outlives the GPU. */
#ifndef AV_CONTEXT_SESSION_HPP
#define AV_CONTEXT_SESSION_HPP

#include "core/log/driver_log.hpp"
#include "core/support/status.hpp"
#include "device/host/stdio_log.hpp"
#include "executor/import_table.hpp"
#include "executor/shim/host_heap.hpp"
#include "proto/wire/spill_block.hpp"
#include "proto/wire/wire_arena.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuClockHold;
class HostMemorySource;
class SessionBackend;

class ContextSession {
public:
    /* The block one command decodes into. The generated decoders allocate
     * every indirect argument - arrays, strings, chained structs - from it,
     * and every count that sizes one comes off the wire, so there is no
     * largest command to derive a size from: this is what most commands
     * need, sized from cost. A megabyte across the sixty four sessions the
     * device can ever ask for is sixty four megabytes of address space; a
     * command that needs more - a program's source of several - takes chunks
     * from the heap for as long as it decodes and runs (WireArena), and one
     * the heap cannot serve is refused and counted (starved()). The block is
     * handed in rather than held here, because sixty four megabytes is not
     * something to carry inside the object that owns the sessions. */
    static constexpr size_t arena_bytes = 1u << 20;

    ContextSession();
    ContextSession(const ContextSession &) = delete;
    ContextSession &operator=(const ContextSession &) = delete;

    /* Binds this session to a backend and to the block its commands decode
     * into. invalid_state if it is already bound. */
    [[nodiscard]] Status initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes);
    /* Releases the session for reuse: what it imported given back, then its
     * backend shut down (SessionBackend::shut_down). Safe on one that never
     * ran. */
    void                 release();
    bool                 live() const;
    /* Whether a command broke it: it then refuses every command, and shares
     * and opens nothing (feature 053). */
    bool                 faulted() const;
    /* The clocks of the GPU this session runs on, told of every batch. */
    void                 hold_clocks(GpuClockHold &clocks);
    /* What this session imports from: its GPU's memory source, under its
     * device's domain (docs/host-memory.md). */
    void                 draw_from(HostMemorySource &source, uint32_t domain);

    /* Runs one command and answers one of the ABI's four results. `reply` may
     * be null with a capacity of zero, which is what the device passes for
     * every command that did not ask for an answer; `answer` is then the
     * length the answer would have been, which is what lets the device tell
     * the guest what to allocate and ask again. */
    void     end_batch();
    /* AV_POLL_*: lost once the backend says its host device is, and for
     * good - the session then refuses every command, as after a command
     * found it lost. */
    uint32_t poll();
    /* What the device gives the library to call when a host thread finishes
     * work; nothing without a backend. */
    void     bind_signal(void (*signal)(void *context), void *context);
    void     gpu_progress(uint64_t &marked, uint64_t &reached) const;
    /* An answer longer than `reply_capacity` writes nothing there and is
     * kept, until the next execute, for answer_part. */
    uint32_t execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                     size_t reply_capacity, size_t &answer);
    /* The kept answer from `offset`, as much as `capacity` takes copied to
     * `out` and counted in the return; `length` is the whole answer's, 0
     * with nothing kept. */
    size_t   answer_part(uint64_t offset, uint8_t *out, size_t capacity, uint64_t &length) const;

    /* Makes the memory under `memory_key` usable by this session's commands
     * as the import `blob_id`. AV_EXECUTED, or AV_UNDECODABLE for a key with
     * no memory, an id already imported, a table that is full, or an API
     * that will not take the memory. */
    uint32_t import_blob(uint64_t blob_id, uint64_t memory_key);
    /* Drops one. Nothing for an id that was never imported, because the device
     * may release one whose import failed. */
    void     release_blob(uint64_t blob_id);
    /* The number the guest names what the import of `blob_id` made by, or 0:
     * for an id never imported, one released, and memory the guest already
     * freed. */
    uint64_t imported_handle(uint64_t blob_id);
    /* What this session imported, which its backend finds the imports its
     * commands name in: bound to it as the session opens. */
    ImportTable &imports();
    /* For tests: the host device this session runs on made lost
     * (SessionBackend::lose_device). AV_EXECUTED, or AV_UNKNOWN where the
     * host cannot, or with no backend. */
    uint32_t lose_device();


    /* Commands this session refused because the block was too small rather
     * than because the frame was bad. The guest is told the same thing either
     * way; this is what an operator sees. */
    size_t   starved() const;

    bool invariants_hold() const;

private:
    friend struct ContextSessionProbe;

    SessionBackend *backend_;
    GpuClockHold   *clocks_;
    HostMemorySource *source_;
    uint32_t        domain_;
    /* Where the arena's chunks and a long answer come from. */
    HostHeap        heap_;
    WireArena       arena_;
    /* The answer of the last command, when it outgrew the device's reply. */
    SpillBlock      answer_;
    size_t          answer_length_;
    ImportTable     imports_;
    size_t          starved_;
    bool            faulted_;
    /* QEMU's stderr: a command or a blob refused here ends the context, and the
     * device says only which batch it was in. */
    StdioLog        sink_;
    DriverLog       lines_;
};

}  // namespace av

#endif  // AV_CONTEXT_SESSION_HPP
