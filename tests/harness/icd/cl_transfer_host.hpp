#ifndef AV_CL_TRANSFER_HOST_HPP
#define AV_CL_TRANSFER_HOST_HPP

#include "proto/client/client_blob_arena.hpp"
#include "harness/transport/roomed_transport.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ClTransferHost final : public RoomedTransport {
public:
    static constexpr std::uint64_t buffer_handle = 0x6600u;
    static constexpr std::uint64_t image_handle = 0x6601u;
    static constexpr std::size_t   buffer_bytes = 4u << 20;
    static constexpr std::size_t   image_width = 1u << 18;
    static constexpr std::size_t   image_height = 16u;
    static constexpr std::size_t   image_depth = 3u;
    static constexpr std::size_t   element_bytes = 4u;
    static constexpr std::uint32_t max_commands = 4096u;

    struct Command {
        std::uint32_t id;
        std::uint64_t offset;
        std::uint64_t bytes;
        std::uint64_t origin[3];
        std::uint64_t region[3];
        std::uint64_t row_pitch;
        std::uint64_t buffer_pitch[2];
        std::uint64_t host_origin[3];
        bool          blocking;
        bool          lent;
    };

    ClTransferHost();
    ~ClTransferHost();
    ClTransferHost(const ClTransferHost &) = delete;
    ClTransferHost &operator=(const ClTransferHost &) = delete;

    void           fill(std::uint8_t seed);
    void           lend(std::uint8_t *block, std::size_t bytes);
    std::uint8_t   buffer_at(std::size_t offset) const;
    std::uint32_t  texel(std::size_t x, std::size_t y, std::size_t z) const;
    std::uint32_t  texel_of(std::size_t x, std::size_t y, std::size_t z) const;
    void           refuse(std::uint32_t index, std::int32_t error);
    std::uint32_t  commands() const;
    const Command &command(std::uint32_t index) const;

    WireWriter      request_writer() override;
    [[nodiscard]] TransportResult send_async(std::uint32_t command_id, WireWriter &writer) override;
    [[nodiscard]] TransportResult send_sync(std::uint32_t command_id, WireWriter &writer,
                                            WireReader &reply) override;
    void             end_call() override;
    WireArena       &decode_arena() override;
    ClientBlobArena &blobs() override;

private:
    static constexpr std::size_t request_bytes = 256u * 1024u;
    static constexpr std::size_t reply_bytes = 1024u * 1024u;
    static constexpr std::size_t scratch_bytes = request_bytes > reply_bytes ? request_bytes : reply_bytes;
    static constexpr std::size_t image_bytes = image_width * image_height * image_depth * element_bytes;

    bool apply(std::uint32_t command_id, WireWriter &writer);
    bool buffer_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    bool image_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    bool rect_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    std::size_t rect_offset(const Command &seen, std::size_t x, std::size_t y, std::size_t z) const;
    std::size_t image_offset(std::size_t x, std::size_t y, std::size_t z) const;
    bool        host_bytes(WireReader &payload, std::uint8_t present, std::uint64_t count, Command &seen,
                           std::uint8_t *&at);

    std::uint8_t   *request_;
    std::uint8_t   *reply_;
    std::uint8_t   *buffer_;
    std::uint8_t   *image_;
    std::uint8_t   *scratch_;
    std::uint8_t   *lent_;
    std::size_t     lent_bytes_;
    Command        *seen_;
    std::uint32_t   commands_;
    std::size_t     answered_;
    std::uint32_t   refused_at_;
    std::int32_t    refusal_;
    WireArena       arena_;
    ClientBlobArena blobs_;
};

}

#endif
