/* The host buffer a context's pixel transfer goes through when its bytes
 * cannot cross with its command: more than one command carries inline, and
 * no blob arena to lend them whole (docs/host-memory.md). The command then
 * names offset 0 of this buffer, bound at the pixel buffer target of its
 * direction as an application's own would be, and the host applies the same
 * pixel store to it; the bytes cross to the buffer or from it in pieces
 * (BufferPieces). Only a transfer the guest knows has no pixel buffer bound
 * comes here, so the binding goes back to none after it.
 *
 * One buffer per context, named the first time one is needed and given new
 * storage of each transfer's size, deleted with the context - or forgotten
 * with a context lost, whose host objects went with it. */
#ifndef AV_GLICD_PIXEL_STAGING_BUFFER_HPP
#define AV_GLICD_PIXEL_STAGING_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}  // namespace av

namespace av::glicd {

struct PixelStagingBufferProbe;

class PixelStagingBuffer {
public:
    enum : uint32_t {
        gl_pixel_pack_buffer = 0x88EBu, gl_pixel_unpack_buffer = 0x88ECu, gl_stream_draw = 0x88E0u,
        gl_stream_read = 0x88E1u,
    };
    PixelStagingBuffer();
    PixelStagingBuffer(const PixelStagingBuffer &) = delete;
    PixelStagingBuffer &operator=(const PixelStagingBuffer &) = delete;

    /* Before an upload of `bytes` at `pixels`: true when they went into the
     * buffer, bound at GL_PIXEL_UNPACK_BUFFER, and the command is to name
     * offset 0 - false when they cross with it, there are none, or the host
     * named no buffer. */
    [[nodiscard]] bool stage_upload(ClientTransport &wire, const void *pixels, size_t bytes);
    /* After the staged upload's command: the binding back to none. */
    void               finish_upload(ClientTransport &wire);
    /* Before a download of `bytes` into `pixels`: true when the buffer, of
     * that size, is bound at GL_PIXEL_PACK_BUFFER for the command. */
    [[nodiscard]] bool stage_download(ClientTransport &wire, const void *pixels, size_t bytes);
    /* After the staged download's command: its bytes into `pixels`, and the
     * binding back to none. */
    void               finish_download(ClientTransport &wire, void *pixels, size_t bytes);
    /* The host's buffer deleted; the next transfer that needs one names it
     * again. */
    void               release(ClientTransport &wire);
    /* The context went with its host objects: nothing is held. */
    void               forget();
    /* The host's name for the buffer; 0 before the first staged transfer. */
    uint32_t           name() const;
    bool               invariants_hold() const;

private:
    enum class Staged : uint8_t { nothing, upload, download };

    /* The buffer named and bound at `target` with storage of `bytes`;
     * false when the host named none. */
    [[nodiscard]] bool bind_storage(ClientTransport &wire, uint32_t target, size_t bytes, uint32_t usage);

    uint32_t name_;
    Staged   staged_;

    friend struct PixelStagingBufferProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_PIXEL_STAGING_BUFFER_HPP
