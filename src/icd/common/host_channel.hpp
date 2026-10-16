/* One av context of a driver's to the host, opened over a device's
 * connection and closed in the one order that is safe: the staging a command
 * is written in, the KmtTransport that sends it, the block it holds a
 * driver's commands in when the driver batches, and the blob arena bulk data
 * crosses through. OpenGL's and OpenCL's contexts, a Direct3D session and a
 * Vulkan instance each hold one; what they lay over it - a command stream, a
 * pacer - is theirs.
 *
 * The channel's send lock is the one every thread sending on it takes: the
 * wire the generated clients are handed takes it per command (LockedTransport),
 * and what the channel sends itself - memory, a batch, the publish barrier -
 * takes it whole. A context only ever sent on from one thread at a time - an
 * OpenGL context, current on one thread - is handed the transport unlocked
 * as well.
 *
 * Opening is all or nothing. An arena the device cannot give is asked again
 * at half the size down to KmtBlobArena::default_bytes, and none at all is a
 * channel whose bulk data crosses inline - unless the transport faulted while
 * asking, which is a context already lost. A lost channel is closed without
 * asking the host anything: its device frees what it held with the context. */
#ifndef AV_ICD_HOST_CHANNEL_HPP
#define AV_ICD_HOST_CHANNEL_HPP

#include "proto/resources/host_memory_request.hpp"

#include "core/support/object_pool.hpp"

#include "guest/transport/host_allocation.hpp"

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/retire_list.hpp"
#include "icd/common/locked_transport.hpp"
#include "core/log/driver_log.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/sub_api.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class GuestConnection;
class StateWriter;
}  // namespace av

namespace av::icd {

class ThreadLock;
struct HostChannelProbe;

/* The sizes one driver's channels are opened at. */
class ChannelShape {
public:
    /* A `batch_bytes` of 0 sends each command alone; an `arena_bytes` of 0
     * opens no arena. */
    ChannelShape(SubApi sub_api, uint8_t ring_order, uint32_t reply_bytes, size_t staging_bytes, size_t batch_bytes,
                 uint64_t arena_bytes);

private:
    friend class HostChannel;

    SubApi   sub_api_;
    uint8_t  ring_order_;
    uint32_t reply_bytes_;
    size_t   staging_bytes_;
    size_t   batch_bytes_;
    uint64_t arena_bytes_;
};

class HostChannel {
public:
    /* `prefix` is the driver's, for the lines the channel writes; `lock` is
     * its send lock, which may be other channels' too. */
    HostChannel(BlockSource &memory, LogSink &log, const char *prefix, const ChannelShape &shape, ThreadLock &lock);
    /* Closed as a lost channel: nothing is left to tell the host. */
    ~HostChannel();
    HostChannel(const HostChannel &) = delete;
    HostChannel &operator=(const HostChannel &) = delete;

    /* Opens over `device`, its context made with `options`. invalid_state
     * when open or when the transport faulted opening the arena; no_memory
     * when the staging or batch cannot be had; otherwise the transport's
     * refusal. Nothing is left open unless ok. */
    [[nodiscard]] Status open(GuestConnection &device, const GlContextOptions &options = GlContextOptions());
    /* Sends what the transport holds unless `lost`, then closes the arena -
     * or forgets it, when `lost` - and the transport. Nothing when closed. */
    void                 close(bool lost);

    bool opened() const;
    bool faulted() const;
    /* What the generated clients send through, under the send lock: only
     * while open. */
    ClientTransport    &wire();
    /* The transport itself, with no lock - for a thread holding it, or a
     * channel only one thread sends on: only while open. It holds what is
     * sent asynchronously on a shape that batches. */
    KmtTransport       &transport();
    const KmtTransport &transport() const;
    /* Under the send lock: what the transport holds, sent (KmtTransport::
     * flush); `length` framed commands, none answered, published behind it
     * (send_batch); and the publish barrier (run_held). Each fails as the
     * transport does, and on a channel not open. */
    [[nodiscard]] TransportResult flush();
    [[nodiscard]] TransportResult send_batch(const uint8_t *framed, size_t length);
    [[nodiscard]] Status          run_held();
    /* The command `writer` wrote - one a recording lent memory of its own,
     * too long for any batch it frames - sent asynchronously under the send
     * lock, so its pieces cross as one run that nothing else of the context
     * comes between (docs/protocol.md, "Long commands"). */
    [[nodiscard]] TransportResult send_command(uint32_t command_id, WireWriter &writer);
    /* The arena this process sees and its bytes; null and 0 without one. */
    uint8_t          *arena_view() const;
    uint64_t          arena_bytes() const;
    /* The arena's error word, which the host writes and never lends; 0
     * without an arena. */
    uint64_t          host_errors() const;

    /* `bytes` of the host memory `memory` names, opened on this channel's
     * context and imported (docs/host-memory.md): one allocation for one
     * object - a Vulkan allocation, a Direct3D 12 heap, Direct3D 11's slice
     * store - which the channel keeps until close_memory or its own close.
     * invalid_state when not open or faulted; no_memory with no room to keep
     * it; otherwise HostAllocation's refusal, with nothing kept. Under the
     * send lock, as are close_memory and find_memory. */
    [[nodiscard]] Status open_memory(uint64_t bytes, HostMemoryRequest memory, HostAllocation *&made);
    /* The `bytes` of the process's own memory at `address` (whole pages,
     * writable) lent to the device as host memory of the kind `memory` names
     * and imported (HostAllocation::adopt, docs/host-memory.md, "Application
     * memory"), kept as open_memory keeps what it makes, with its answers. */
    [[nodiscard]] Status adopt_memory(void *address, uint64_t bytes, HostMemoryRequest memory,
                                      HostAllocation *&made);
    /* What open_memory made, released - abandoned on a faulted channel, whose
     * context frees it on the host. The release's refusal, if any; the
     * allocation is gone either way. */
    [[nodiscard]] Status close_memory(HostAllocation &allocation);
    /* What open_memory or adopt_memory made, which work the host queued may
     * still read or write, closed only once the API says that work is done
     * (reclaim_retired); with no memory to note it, closed now, and logged.
     * The rule every API's application memory keeps (docs/host-memory.md,
     * "Application memory"). */
    void                 retire_memory(HostAllocation &allocation);
    /* Every retired allocation closed: the host's work over them is done. */
    void                 reclaim_retired();
    /* The allocation the host named `handle`, or null. */
    HostAllocation      *find_memory(uint64_t handle);
    uint32_t             memories() const;

    /* Its fields on the line `writer` has open. */
    void report(StateWriter &writer) const;
    /* Closed holds no memory; open holds the staging and a sound transport
     * behind its lock, which holds commands in the batch block exactly when
     * the shape batches. The arena is only ever made over an open transport. */
    bool invariants_hold() const;

private:
    friend struct HostChannelProbe;

    /* The arena at the shape's size, halved while the device refuses, down to
     * the least it is ever opened at. */
    [[nodiscard]] Status open_arena(GuestConnection &device);
    /* A fresh slot for an allocation: invalid_state when not open or faulted,
     * no_memory with no room. */
    [[nodiscard]] Status keep(ObjectStorage<HostAllocation> *&kept);
    /* `kept` answered as `made` when `taken` is ok, given up otherwise. */
    [[nodiscard]] Status settle(ObjectStorage<HostAllocation> *kept, Status taken, HostAllocation *&made);
    /* Every allocation open_memory made, released or abandoned. */
    void                 close_memories(bool lost);

    BlockSource                   &memory_;
    DriverLog                      lines_;
    ThreadLock                    &lock_;
    SubApi                         sub_api_;
    uint8_t                        ring_order_;
    uint32_t                       reply_bytes_;
    size_t                         staging_bytes_;
    size_t                         batch_bytes_;
    uint64_t                       arena_bytes_;
    uint8_t                       *staging_;
    uint8_t                       *batch_block_;
    ObjectStorage<KmtTransport>     transport_;
    ObjectStorage<LockedTransport>  locked_;
    ObjectStorage<KmtBlobArena>     arena_;
    /* The connection the channel was opened over, which an allocation is
     * opened on too; null while closed. */
    GuestConnection                *device_;
    ObjectPool<ObjectStorage<HostAllocation>> memories_;
    /* What was let go of while the host may still use it (retire_memory). */
    RetireList<HostAllocation *>              retired_;
};

}  // namespace av::icd

#endif  // AV_ICD_HOST_CHANNEL_HPP
