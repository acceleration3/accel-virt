/* One process's Direct3D commands, batched until they must go.
 *
 * The generated D3D client encodes each command through this transport; an
 * asynchronous one is held by the session's transport (KmtTransport::hold_in),
 * and nothing crosses to the host until the batch is full, the runtime
 * flushes, or a command needs an answer - then everything held so far goes in
 * one submission, the one asking included, so the host has run all of it when
 * the answer comes back. A draw call is a few dozen bytes; one submission per
 * draw would be one kernel transition per draw. The transport holds them,
 * not this, so that what the session sends on it of its own - a heap's blob
 * import - goes in order behind them.
 *
 * Bulk data - a texture upload, a readback - crosses through the transport's
 * blob arena rather than the batch. A lease comes back once the host has run
 * the batch that carried its command, so when the arena runs short the
 * device asks for room (make_room_for_blobs), which sends what is held and
 * waits for the oldest batches.
 *
 * Every device the process makes speaks on this one stream, so the doorbell
 * and ring it costs are spent once per process: the stream remembers which
 * device the host has selected and selects another only when the speaker
 * changes (speak_as), which inside a batch is rare. A deferred context's
 * stream records into a block of its own instead (record), which no
 * transport sees until the command list runs. */
#ifndef AV_D3D_COMMAND_STREAM_HPP
#define AV_D3D_COMMAND_STREAM_HPP

#include "core/support/status.hpp"
#include "guest/transport/framed_block.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class KmtTransport;
class StateWriter;
}  // namespace av

namespace av::d3d {

class CommandRecording;

class D3dCommandStream final : public ClientTransport {
public:
    /* The most one command is lent in a recording, which bounds its inline
     * data; a sending stream's command is lent the transport's room, and one
     * that outgrows it spills and is sent in pieces (docs/protocol.md, "Long
     * commands"). */
    static constexpr size_t command_room = 256u * 1024u;
    /* The most data one recorded command carries: a recording has no arena,
     * so its data goes inline, and half a command's room leaves the rest for
     * the command's other fields. */
    static constexpr size_t recorded_data = command_room / 2u;
    /* What is recorded before an idle host is sent it: a few dozen draws, so
     * a submission's cost is spread over enough of them to stay small. */
    static constexpr size_t stream_bytes = 1024u;

    /* Sends on `transport`, which holds what is sent asynchronously. */
    explicit D3dCommandStream(KmtTransport &transport);
    D3dCommandStream(const D3dCommandStream &) = delete;
    D3dCommandStream &operator=(const D3dCommandStream &) = delete;

    /* The commands that select a speaker and pass on an application's
     * flush: Direct3D 11's (SelectDevice11, Flush11) until told otherwise. A
     * Direct3D 9 session's stream speaks for Direct3D 9 devices
     * (SelectDevice9, Flush9): Direct3D 11's would name to the host a device
     * it keeps no Direct3D 11 state for, which it refuses. A u32 device, and
     * nothing. */
    void                 speak_with(uint32_t select_command, uint32_t flush_command);

    /* Sends what is held, answered by nothing. invalid_state when the
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
    /* The publish barrier (KmtTransport::run_held): what is held is sent and
     * run by the host when this returns. invalid_state for a recording, which
     * sends nothing, and when the transport failed, which loses the device. */
    [[nodiscard]] Status run_held();
    /* From here on nothing is sent: commands collect in `recording` for a
     * deferred context's command list, carrying their data inline, and a
     * command that must be answered fails. Before anything is recorded. */
    void                 record(CommandRecording &recording);
    /* What was recorded since the last take, copied to `out` (at most
     * `capacity` bytes) and forgotten. invalid_parameter when it does not fit;
     * no_space, with the recording forgotten, when a command was lost from it
     * - one past the end of the block, or one whose data did not fit. */
    [[nodiscard]] Status take_recorded(uint8_t *out, size_t capacity, size_t &length);
    size_t               recorded_bytes() const;
    /* Bytes recorded since the stream was made: equal twice, nothing came between. */
    uint64_t             recorded_total() const;
    /* Recorded commands, run in this stream's order: copied in a block's worth
     * of whole commands at a time, what is pending sent to make room. malformed
     * when they are not whole commands; no_space when a recording cannot grow
     * to hold them. */
    [[nodiscard]] Status append_recorded(const uint8_t *bytes, size_t length);
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
    /* The transport's (KmtTransport::make_room_for_blobs). A recording's data
     * goes inline instead: false when `bytes` is more than one recorded
     * command carries. */
    [[nodiscard]] bool make_room_for_blobs(size_t bytes) override;
    const TransferRoom &room() const override;

    bool invariants_hold() const;

private:
    /* Held by the transport while sending, framed into the recording while
     * recording. */
    [[nodiscard]] TransportResult append(uint32_t command_id, WireWriter &writer);
    /* The block is the recording's, which may have grown. */
    void                          adopt_recording();
    /* Room for `wanted` more bytes in the recording, which grows. False when
     * it cannot. */
    [[nodiscard]] bool            room_for(size_t wanted);
    /* How many bytes of the commands at `bytes` make a run of whole commands
     * that fits the recording's block, or the transport's: 0 when not even
     * the first does or is one. */
    size_t                        whole_commands(const uint8_t *bytes, size_t length) const;

    KmtTransport     &transport_;
    /* The recording's commands; no block while sending. */
    FramedBlock       framed_;
    uint64_t      batches_;
    uint64_t      recorded_;
    /* recorded_ just after the last application flush was recorded. */
    uint64_t      flushed_at_;
    /* Never ready: a recording's data goes inline, since the lease an arena
     * hands out is gone before a command list runs, which may be many times. */
    ClientBlobArena no_arena_;
    /* What commands collect in instead of being sent; null while sending. */
    CommandRecording *recording_;
    /* A recording lent a writer and its command is not yet appended: one the
     * call ends without was abandoned. */
    bool          lent_;
    /* A recorded command was lost: the recording is incomplete. */
    bool          incomplete_;
    /* The device the host runs recorded commands on; 0 before any. */
    uint32_t      speaking_;
    uint32_t      select_command_;
    uint32_t      flush_command_;
    bool          failed_;
};

}  // namespace av::d3d

#endif  // AV_D3D_COMMAND_STREAM_HPP
