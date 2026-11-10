#ifndef AV_GLICD_DEBUG_DELIVERY_TRANSPORT_HPP
#define AV_GLICD_DEBUG_DELIVERY_TRANSPORT_HPP

#include "proto/client/client_transport.hpp"

#include <stdint.h>

namespace av::glicd {

class GlDebugMessages;

class DebugDeliveryTransport final : public ClientTransport {
public:
    explicit DebugDeliveryTransport(GlDebugMessages &messages);

    void bind(ClientTransport &inner);

    WireWriter         request_writer() override;
    WireWriter         request_writer_with_room(const uint64_t *sizes, uint32_t count) override;
    TransportResult    send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult    send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void               end_call() override;
    WireArena         &decode_arena() override;
    ClientBlobArena   &blobs() override;
    [[nodiscard]] bool make_room_for_blobs(const uint64_t *sizes, uint32_t count) override;
    ClientBlobLease    open_region(uint64_t bytes) override;
    void               close_region(const ClientBlobLease &region) override;
    const TransferRoom &room() const override;
    void               note(ChannelCounter counter, uint64_t amount) override;

    bool invariants_hold() const;

private:
    void deliver();

    GlDebugMessages &messages_;
    ClientTransport *inner_;
};

}

#endif
