/* One process's Direct3D commands, batched until they must go.
 *
 * The generated D3D client encodes each command through this transport; an
 * asynchronous one is framed and appended to the batch, and nothing crosses to
 * the host until the batch is full, the runtime flushes, or a command needs an
 * answer - then everything recorded so far goes in one submission, the one
 * asking included, so the host has run all of it when the answer comes back.
 * A draw call is a few dozen bytes; one submission per draw would be one
 * kernel transition per draw.
 *
 * Bulk data - a texture upload, a readback - crosses through the transport's
 * blob arena rather than the batch. A lease is only retired once the host
 * has answered a command sent after it, so when the arena runs short the
 * device asks for room (make_room_for), which answers a round trip.
 *
 * Every device the process makes speaks on this one stream, so the doorbell
 * and ring it costs are spent once per process: the stream remembers which
 * device the host has selected and selects another only when the speaker
 * changes (speak_as), which inside a batch is rare. */
#ifndef AV_D3D_COMMAND_STREAM_HPP
#define AV_D3D_COMMAND_STREAM_HPP

#include "core/support/status.hpp"
#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class KmtTransport;
class StateWriter;
}  // namespace av

namespace av::d3d {

class D3dCommandStream final : public ClientTransport {
public:
    /* The most one command is lent, which bounds inline data too: anything
     * larger goes through the blob arena. */
    static constexpr size_t command_room = 256u * 1024u;

    /* Records into `capacity` bytes at `block`, which the caller keeps and
     * which must hold at least one command's room past its header. */
    D3dCommandStream(KmtTransport &transport, uint8_t *block, size_t capacity);
    D3dCommandStream(const D3dCommandStream &) = delete;
    D3dCommandStream &operator=(const D3dCommandStream &) = delete;

    /* Sends what is recorded, answered by nothing. invalid_state when the
     * transport refused it, which loses the device; ok with nothing to send. */
    [[nodiscard]] Status flush();
    /* What an application's Flush asks for: the batch goes when the host is
     * ready for it, and otherwise keeps growing until a later flush, a sync
     * or a full block sends it. */
    [[nodiscard]] Status flush_hint();
    /* An application's Flush: the host flushes its own queue too, unless
     * nothing was recorded since it last did - GetData asks for one on every
     * poll. */
    [[nodiscard]] Status application_flush();
    /* From here on nothing is sent: commands collect in the block for a
     * deferred context's command list, carrying their data inline, and a
     * command that must be answered fails. */
    void                 record();
    /* What was recorded since the last take, copied to `out` (at most
     * `capacity` bytes) and forgotten. invalid_parameter when it does not fit. */
    [[nodiscard]] Status take_recorded(uint8_t *out, size_t capacity, size_t &length);
    size_t               recorded_bytes() const;
    /* Bytes recorded since the stream was made: equal twice, nothing came between. */
    uint64_t             recorded_total() const;
    /* Recorded commands, run in this stream's order: copied in when they fit
     * the block, sent as a batch of their own after what is pending when not. */
    [[nodiscard]] Status append_recorded(const uint8_t *bytes, size_t length);
    /* The blob arena has `bytes` free, retiring every lease with a round trip
     * when it has not. False when even an empty arena is too small. */
    [[nodiscard]] bool   make_room_for(size_t bytes);
    /* Commands recorded after this run on `device`. False when the selection
     * could not be sent, which fails the stream. Device 0 selects nothing. */
    [[nodiscard]] bool   speak_as(uint32_t device);
    /* The host selected `device` itself, as CreateDevice11 does. */
    void                 selected(uint32_t device);
    /* `device` is gone on the host, so it is no longer the one selected. */
    void                 forget(uint32_t device);
    /* A device-side failure latched: every later send fails. */
    bool                 failed() const;
    /* Batches sent and bytes recorded since the device was made. */
    void                 report(StateWriter &writer) const;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

    bool invariants_hold() const;

private:
    [[nodiscard]] TransportResult append(uint32_t command_id, WireWriter &writer, uint8_t flags);

    KmtTransport &transport_;
    uint8_t      *block_;
    size_t        capacity_;
    size_t        used_;
    uint64_t      batches_;
    uint64_t      recorded_;
    /* recorded_ just after the last application flush was recorded. */
    uint64_t      flushed_at_;
    /* Never ready: a recording's data goes inline, since the lease an arena
     * hands out is gone before a command list runs, which may be many times. */
    ClientBlobArena no_arena_;
    bool          recording_;
    /* The device the host runs recorded commands on; 0 before any. */
    uint32_t      speaking_;
    bool          failed_;
};

}  // namespace av::d3d

#endif  // AV_D3D_COMMAND_STREAM_HPP
