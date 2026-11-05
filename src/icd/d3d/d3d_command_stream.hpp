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
}

namespace av::d3d {

class CommandRecording;

class D3dCommandStream final : public ClientTransport {
public:
    static constexpr size_t command_room = 256u * 1024u;
    static constexpr size_t recorded_data = command_room / 2u;
    static constexpr size_t stream_bytes = 1024u;

    explicit D3dCommandStream(KmtTransport &transport);
    D3dCommandStream(const D3dCommandStream &) = delete;
    D3dCommandStream &operator=(const D3dCommandStream &) = delete;

    void                 speak_with(uint32_t select_command, uint32_t flush_command);

    [[nodiscard]] Status flush();
    [[nodiscard]] Status flush_hint();
    [[nodiscard]] Status application_flush();
    [[nodiscard]] Status run_held();
    void                 record(CommandRecording &recording);
    [[nodiscard]] Status take_recorded(uint8_t *out, size_t capacity, size_t &length);
    size_t               recorded_bytes() const;
    uint64_t             recorded_total() const;
    [[nodiscard]] Status append_recorded(const uint8_t *bytes, size_t length);
    [[nodiscard]] bool   speak_as(uint32_t device);
    void                 selected(uint32_t device);
    void                 forget(uint32_t device);
    bool                 failed() const;
    void                 report(StateWriter &writer) const;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;
    [[nodiscard]] bool make_room_for_blobs(const uint64_t *sizes, uint32_t count) override;
    ClientBlobLease    open_region(uint64_t bytes) override;
    void               close_region(const ClientBlobLease &region) override;
    const TransferRoom &room() const override;
    void note(ChannelCounter counter, uint64_t amount) override;

    bool invariants_hold() const;

private:
    [[nodiscard]] TransportResult append(uint32_t command_id, WireWriter &writer);
    void                          adopt_recording();
    [[nodiscard]] bool            room_for(size_t wanted);
    size_t                        whole_commands(const uint8_t *bytes, size_t length) const;

    KmtTransport     &transport_;
    FramedBlock       framed_;
    uint64_t      batches_;
    uint64_t      recorded_;
    uint64_t      flushed_at_;
    ClientBlobArena no_arena_;
    CommandRecording *recording_;
    bool          lent_;
    bool          incomplete_;
    uint32_t      speaking_;
    uint32_t      select_command_;
    uint32_t      flush_command_;
    bool          failed_;
};

}

#endif
