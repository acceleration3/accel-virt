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
#include "proto/wire/wire_arena.hpp"

#include <stddef.h>
#include <stdint.h>

struct AvBlobHandle;

namespace av {

class GpuClockHold;
class SessionBackend;

class ContextSession {
public:
    /* The block one command decodes into. The generated decoders allocate
     * every indirect argument - arrays, strings, chained structs - from it,
     * and every count that sizes one comes off the wire, so there is no
     * largest command to derive a size from: this is a policy, sized from
     * cost. A megabyte across the sixty four sessions the device can ever ask
     * for is sixty four megabytes of address space, and a command that wants
     * more is refused and counted (starved()) rather than served. The block is
     * handed in rather than held here, because sixty four megabytes is not
     * something to carry inside the object that owns the sessions. */
    static constexpr size_t arena_bytes = 1u << 20;

    ContextSession();
    ContextSession(const ContextSession &) = delete;
    ContextSession &operator=(const ContextSession &) = delete;

    /* Binds this session to a backend and to the block its commands decode
     * into. invalid_state if it is already bound. */
    [[nodiscard]] Status initialize(SessionBackend &backend, uint8_t *block, size_t block_bytes);
    /* Releases the session for reuse. Safe on one that never ran. */
    void                 release();
    bool                 live() const;
    /* The clocks of the GPU this session runs on, told of every batch. */
    void                 hold_clocks(GpuClockHold &clocks);

    /* Runs one command and answers one of the ABI's four results. `reply` may
     * be null with a capacity of zero, which is what the device passes for
     * every command that did not ask for an answer; `answer` is then the
     * length the answer would have been, which is what lets the device tell
     * the guest what to allocate and ask again. */
    void     end_batch();
    bool     poll();
    /* What the device gives the library to call when a host thread finishes
     * work; nothing without a backend. */
    void     bind_signal(void (*signal)(void *context), void *context);
    void     gpu_progress(uint64_t &marked, uint64_t &reached) const;
    uint32_t execute(uint32_t command_id, const uint8_t *payload, size_t size, uint8_t *reply,
                     size_t reply_capacity, size_t &answer);

    /* Makes `blob_id` usable by this session's commands. AV_EXECUTED, or
     * AV_UNDECODABLE for a handle the ABI refuses, an id already imported, a
     * table that is full, or an API that cannot take this kind. */
    uint32_t import_blob(uint64_t blob_id, const AvBlobHandle &handle);
    /* Drops one. Nothing for an id that was never imported, because the device
     * may release one whose import failed. */
    void     release_blob(uint64_t blob_id);
    /* The number the guest names what the import of `blob_id` made by, or 0:
     * for an id never imported, one released, and memory the guest already
     * freed. */
    uint64_t imported_handle(uint64_t blob_id);
    uint32_t imports() const;

    /* One host object shared with every other context of this device (feature
     * 053). Whether this API exports or imports one at all is the backend's
     * answer; a faulted session answers no to both, as it refuses every
     * command. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind, int &fd);
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     uint64_t &wire_handle);
    void               release_object(uint32_t kind, uint64_t wire_handle);

    /* Commands this session refused because the block was too small rather
     * than because the frame was bad. The guest is told the same thing either
     * way; this is what an operator sees. */
    size_t   starved() const;

    bool invariants_hold() const;

private:
    friend struct ContextSessionProbe;

    SessionBackend *backend_;
    GpuClockHold   *clocks_;
    WireArena       arena_;
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
