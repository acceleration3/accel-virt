#ifndef AV_ICD_LOCKED_TRANSPORT_HPP
#define AV_ICD_LOCKED_TRANSPORT_HPP

#include "proto/client/client_transport.hpp"

#include <stdint.h>

namespace av {
class ThreadLock;
}

namespace av::icd {

struct LockedTransportProbe;

class LockedTransport final : public ClientTransport {
public:
    LockedTransport(ClientTransport &inner, ThreadLock &lock);

    WireWriter       request_writer() override;
    WireWriter       request_writer_with_room(const uint64_t *sizes, uint32_t count) override;
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
    enum class Phase : uint8_t { idle, writing, answering };

    void let_go();

    ClientTransport &inner_;
    ThreadLock      &lock_;
    Phase            phase_;
    bool             holds_;

    friend struct LockedTransportProbe;
};

}

#endif
