/* A ClientTransport that is an OpenCL host's one buffer and one image, for the
 * transfers the ICD cuts into pieces (ClTransferChunks).
 *
 * It decodes what the generated client sends for clEnqueueReadBuffer,
 * clEnqueueWriteBuffer, clEnqueueReadImage and clEnqueueWriteImage, applies it
 * to storage of its own and answers as the generated server would: a result,
 * and for a read the bytes. So a test states what the buffer or image holds
 * after a transfer, not which commands were sent - and the commands are kept
 * too, each with the extent it named, for the tests about how a transfer was
 * cut. Its request slot is as large as a guest's staging room, so a command
 * that would overflow a guest's overflows here too. */
#ifndef AV_CL_TRANSFER_HOST_HPP
#define AV_CL_TRANSFER_HOST_HPP

#include "proto/client_blob_arena.hpp"
#include "proto/client_transport.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ClTransferHost final : public ClientTransport {
public:
    static constexpr std::uint64_t buffer_handle = 0x6600u;
    static constexpr std::uint64_t image_handle = 0x6601u;
    static constexpr std::size_t   buffer_bytes = 1u << 20;
    /* The image: 4-byte texels, as RGBA8 has. */
    static constexpr std::size_t   image_width = 70000u;
    static constexpr std::size_t   image_height = 16u;
    static constexpr std::size_t   image_depth = 3u;
    static constexpr std::size_t   element_bytes = 4u;
    static constexpr std::uint32_t max_commands = 4096u;

    /* One command as it arrived: which it was, and what it named. */
    struct Command {
        std::uint32_t id;
        std::uint64_t offset;
        std::uint64_t bytes;
        std::uint64_t origin[3];
        std::uint64_t region[3];
        std::uint64_t row_pitch;
        bool          blocking;
    };

    ClTransferHost();
    ~ClTransferHost();
    ClTransferHost(const ClTransferHost &) = delete;
    ClTransferHost &operator=(const ClTransferHost &) = delete;

    /* The buffer's byte i is `seed + i`, the image's texel (x, y, z) its own
     * coordinates, so a read that landed in the wrong place shows. */
    void           fill(std::uint8_t seed);
    std::uint8_t   buffer_at(std::size_t offset) const;
    /* The texel at (x, y, z), as four bytes packed little-endian. */
    std::uint32_t  texel(std::size_t x, std::size_t y, std::size_t z) const;
    std::uint32_t  texel_of(std::size_t x, std::size_t y, std::size_t z) const;
    /* The command numbered `index` (counting from zero) answers `error`. */
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
    static constexpr std::size_t reply_bytes = 64u * 1024u;
    static constexpr std::size_t image_bytes = image_width * image_height * image_depth * element_bytes;

    /* Applies one command and writes its answer; false for one it cannot read. */
    bool buffer_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    bool image_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    std::size_t image_offset(std::size_t x, std::size_t y, std::size_t z) const;

    std::uint8_t   *request_;
    std::uint8_t   *reply_;
    std::uint8_t   *buffer_;
    std::uint8_t   *image_;
    std::uint8_t   *scratch_;
    Command        *seen_;
    std::uint32_t   commands_;
    std::uint32_t   refused_at_;
    std::int32_t    refusal_;
    WireArena       arena_;
    ClientBlobArena blobs_;
};

}  // namespace av

#endif  // AV_CL_TRANSFER_HOST_HPP
