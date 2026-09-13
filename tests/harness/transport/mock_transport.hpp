/* A ClientTransport a test drives without a guest.
 *
 * It lends a writer over its own block, records the command id and bytes of
 * everything sent, and answers a scripted reply. The second implementation of
 * the interface after KmtTransport, and the one that lets the generated Vulkan
 * and OpenCL clients be exercised on a machine with no driver at all (043). */
#ifndef AV_MOCK_TRANSPORT_HPP
#define AV_MOCK_TRANSPORT_HPP

#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class MockTransport final : public ClientTransport {
public:
    static constexpr std::size_t slot_bytes = 65536u;

    MockTransport();

    /* The next send answers `reply` of `length` bytes. */
    void          set_reply(const std::uint8_t *reply, std::size_t length);
    /* The next send fails rather than delivering. */
    void          fail_next();
    std::uint32_t sends() const;
    std::uint32_t last_command() const;
    std::size_t   last_length() const;
    const std::uint8_t *last_payload() const;

    WireWriter request_writer() override;
    [[nodiscard]] TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override;
    [[nodiscard]] TransportResult send_sync(std::uint32_t command_id, WireWriter &writer,
                                            WireReader &reply) override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    TransportResult deliver(std::uint32_t command_id, WireWriter &writer);

    std::uint8_t    request_[slot_bytes];
    std::uint8_t    sent_[slot_bytes];
    std::uint8_t    reply_[slot_bytes];
    std::size_t     reply_length_;
    std::uint32_t   sends_;
    std::uint32_t   last_command_;
    std::size_t     last_length_;
    bool            fail_next_;
    WireArena       arena_;
    ClientBlobArena blobs_;
};

}  // namespace av

#endif  // AV_MOCK_TRANSPORT_HPP
