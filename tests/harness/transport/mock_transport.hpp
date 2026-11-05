#ifndef AV_MOCK_TRANSPORT_HPP
#define AV_MOCK_TRANSPORT_HPP

#include "proto/client/client_blob_arena.hpp"
#include "harness/transport/roomed_transport.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockTransport final : public RoomedTransport {
public:
    static constexpr std::size_t slot_bytes = 65536u;

    MockTransport();

    void          set_reply(const std::uint8_t *reply, std::size_t length);
    void          fail_next();
    std::uint32_t sends() const;
    std::uint32_t end_calls() const;
    std::uint32_t last_command() const;
    std::size_t   last_length() const;
    const std::uint8_t *last_payload() const;

    WireWriter request_writer() override;
    [[nodiscard]] TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override;
    [[nodiscard]] TransportResult send_sync(std::uint32_t command_id, WireWriter &writer,
                                            WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    TransportResult deliver(std::uint32_t command_id, WireWriter &writer);

    std::uint8_t    request_[slot_bytes];
    std::uint8_t    sent_[slot_bytes];
    std::uint8_t    reply_[slot_bytes];
    std::size_t     reply_length_;
    std::uint32_t   sends_;
    std::uint32_t   end_calls_;
    std::uint32_t   last_command_;
    std::size_t     last_length_;
    bool            fail_next_;
    WireArena       arena_;
    ClientBlobArena blobs_;
};

}

#endif
