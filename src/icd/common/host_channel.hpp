/* One av context of a driver's to the host, opened over a device's
 * connection and closed in the one order that is safe: the staging a command
 * is written in, the KmtTransport that sends it, batching for a driver that
 * batches, and the blob arena bulk data crosses through. OpenGL's and
 * OpenCL's contexts, a Direct3D session and a Vulkan instance each hold one;
 * what they lay over it - a lock, a command stream, a pacer - is theirs.
 *
 * Opening is all or nothing. An arena the device cannot give is asked again
 * at half the size down to KmtBlobArena::default_bytes, and none at all is a
 * channel whose bulk data crosses inline - unless the transport faulted while
 * asking, which is a context already lost. A lost channel is closed without
 * asking the host anything: its device frees what it held with the context. */
#ifndef AV_ICD_HOST_CHANNEL_HPP
#define AV_ICD_HOST_CHANNEL_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "guest/transport/kmt_blob_arena.hpp"
#include "guest/transport/kmt_transport.hpp"
#include "icd/common/batched_transport.hpp"
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
    /* `prefix` is the driver's, for the lines the channel writes. */
    HostChannel(BlockSource &memory, LogSink &log, const char *prefix, const ChannelShape &shape);
    /* Closed as a lost channel: nothing is left to tell the host. */
    ~HostChannel();
    HostChannel(const HostChannel &) = delete;
    HostChannel &operator=(const HostChannel &) = delete;

    /* Opens over `device`, its context made with `options`. invalid_state
     * when open or when the transport faulted opening the arena; no_memory
     * when the staging or batch cannot be had; otherwise the transport's
     * refusal. Nothing is left open unless ok. */
    [[nodiscard]] Status open(GuestConnection &device, const GlContextOptions &options = GlContextOptions());
    /* Flushes what is batched unless `lost`, then closes the arena - or
     * forgets it, when `lost` - and the transport. Nothing when closed. */
    void                 close(bool lost);

    bool opened() const;
    bool faulted() const;
    /* Only while open. */
    KmtTransport       &transport();
    const KmtTransport &transport() const;
    /* Only while open, on a shape that batches. */
    BatchedTransport &batched();
    /* The arena this process sees and its bytes; null and 0 without one. */
    uint8_t          *arena_view() const;
    uint64_t          arena_bytes() const;
    /* The arena's error word, which the host writes and never lends; 0
     * without an arena. */
    uint64_t          host_errors() const;

    /* Its fields on the line `writer` has open. */
    void report(StateWriter &writer) const;
    /* Closed holds no memory; open holds the staging and a sound transport,
     * and the batch and its block exactly when the shape batches. The batch
     * and the arena are only ever made over an open transport. */
    bool invariants_hold() const;

private:
    friend struct HostChannelProbe;

    /* The arena at the shape's size, halved while the device refuses, down to
     * the least it is ever opened at. */
    [[nodiscard]] Status open_arena(GuestConnection &device);

    BlockSource                   &memory_;
    DriverLog                      lines_;
    SubApi                         sub_api_;
    uint8_t                        ring_order_;
    uint32_t                       reply_bytes_;
    size_t                         staging_bytes_;
    size_t                         batch_bytes_;
    uint64_t                       arena_bytes_;
    uint8_t                       *staging_;
    uint8_t                       *batch_block_;
    ObjectStorage<KmtTransport>     transport_;
    ObjectStorage<BatchedTransport> batched_;
    ObjectStorage<KmtBlobArena>     arena_;
};

}  // namespace av::icd

#endif  // AV_ICD_HOST_CHANNEL_HPP
