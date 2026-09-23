/* One Direct3D device's commands, batched until they must go.
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
 * device asks for room (make_room_for), which answers a round trip. */
#ifndef AV_D3D_COMMAND_STREAM_HPP
#define AV_D3D_COMMAND_STREAM_HPP

#include "core/support/status.hpp"
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
    /* The blob arena has `bytes` free, retiring every lease with a round trip
     * when it has not. False when even an empty arena is too small. */
    [[nodiscard]] bool   make_room_for(size_t bytes);
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
    bool          failed_;
};

}  // namespace av::d3d

#endif  // AV_D3D_COMMAND_STREAM_HPP
