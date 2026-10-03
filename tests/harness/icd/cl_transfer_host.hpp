/* A ClientTransport that is an OpenCL host's one buffer and one image, for the
 * transfers the ICD cuts into pieces (ClTransferChunks).
 *
 * It decodes what the generated client sends for clEnqueueReadBuffer,
 * clEnqueueWriteBuffer, clEnqueueReadImage, clEnqueueWriteImage and the
 * buffer rectangles clEnqueueReadBufferRect and clEnqueueWriteBufferRect, applies it
 * to storage of its own and answers as the generated server would: a result,
 * and for a read the bytes. So a test states what the buffer or image holds
 * after a transfer, not which commands were sent - and the commands are kept
 * too, each with the extent it named, for the tests about how a transfer was
 * cut. Its request slot is as large as a guest's staging room, so a command
 * that would overflow a guest's overflows here too.
 *
 * Memory it is lent (`lend`) is a context's blob arena: a command that names
 * its bytes there by offset has them read from it, or written into it, as the
 * host's OpenCL would through lent_for, and its answer carries no bytes. */
#ifndef AV_CL_TRANSFER_HOST_HPP
#define AV_CL_TRANSFER_HOST_HPP

#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"

#include <cstddef>
#include <cstdint>

namespace av {

class ClTransferHost final : public ClientTransport {
public:
    static constexpr std::uint64_t buffer_handle = 0x6600u;
    static constexpr std::uint64_t image_handle = 0x6601u;
    /* Room for a read of several replies' worth. */
    static constexpr std::size_t   buffer_bytes = 4u << 20;
    /* The image: 4-byte texels, as RGBA8 has, and a row longer than a reply. */
    static constexpr std::size_t   image_width = 1u << 18;
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
        /* A buffer rectangle's own: its row and slice pitch in the buffer, and
         * the application's corner, which the ICD always sends as zero. */
        std::uint64_t buffer_pitch[2];
        std::uint64_t host_origin[3];
        bool          blocking;
        /* Whether its bytes were named by offset in the lent memory. */
        bool          lent;
    };

    ClTransferHost();
    ~ClTransferHost();
    ClTransferHost(const ClTransferHost &) = delete;
    ClTransferHost &operator=(const ClTransferHost &) = delete;

    /* The buffer's byte i is `seed + i`, the image's texel (x, y, z) its own
     * coordinates, so a read that landed in the wrong place shows. */
    void           fill(std::uint8_t seed);
    /* `bytes` at `block` are the blob arena from now on, for the client's
     * leases and for commands that name an offset in it. */
    void           lend(std::uint8_t *block, std::size_t bytes);
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
    /* A context's staging room and its reply slot, which bound what one
     * command carries each way. */
    static constexpr std::size_t request_bytes = 256u * 1024u;
    static constexpr std::size_t reply_bytes = 1024u * 1024u;
    /* Holds a write's bytes or a read's, whichever is larger. */
    static constexpr std::size_t scratch_bytes = request_bytes > reply_bytes ? request_bytes : reply_bytes;
    static constexpr std::size_t image_bytes = image_width * image_height * image_depth * element_bytes;

    /* Applies the command `writer` framed, its answer left in the reply slot;
     * false for one it cannot read. */
    bool apply(std::uint32_t command_id, WireWriter &writer);
    /* Applies one command and writes its answer; false for one it cannot read. */
    bool buffer_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    bool image_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    bool rect_command(std::uint32_t id, WireReader &payload, Command &seen, WireWriter &answer);
    /* Where byte `x` of row `y` of slice `z` of a rectangle lies in the buffer. */
    std::size_t rect_offset(const Command &seen, std::size_t x, std::size_t y, std::size_t z) const;
    std::size_t image_offset(std::size_t x, std::size_t y, std::size_t z) const;
    /* Where a command's `count` bytes are, after its presence byte and count:
     * the lent memory at the offset that follows, or the scratch block for
     * bytes inline; false for none or an offset outside what was lent. */
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

}  // namespace av

#endif  // AV_CL_TRANSFER_HOST_HPP
