#ifndef AV_ICD_UNANSWERED_TRANSPORT_HPP
#define AV_ICD_UNANSWERED_TRANSPORT_HPP

#include "proto/client/client_transport.hpp"

#include <stdint.h>

namespace av::icd {

class UnansweredTransport final : public ClientTransport {
public:
    explicit UnansweredTransport(ClientTransport &inner);
    UnansweredTransport(const UnansweredTransport &) = delete;
    UnansweredTransport &operator=(const UnansweredTransport &) = delete;

    WireWriter       request_writer() override;
    TransportResult  send_async(uint32_t command_id, WireWriter &writer) override;
    TransportResult  send_sync(uint32_t command_id, WireWriter &writer, WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;
    [[nodiscard]] bool make_room_for_blobs(size_t bytes) override;
    const TransferRoom &room() const override;
    void note(ChannelCounter counter, uint64_t amount) override;

private:
    ClientTransport &inner_;
    uint8_t          success_[1];
};

}

#endif
