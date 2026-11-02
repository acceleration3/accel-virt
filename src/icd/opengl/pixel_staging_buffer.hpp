#ifndef AV_GLICD_PIXEL_STAGING_BUFFER_HPP
#define AV_GLICD_PIXEL_STAGING_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class ClientTransport;
}

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

    [[nodiscard]] bool stage_upload(ClientTransport &wire, const void *pixels, size_t bytes);
    void               finish_upload(ClientTransport &wire);
    [[nodiscard]] bool stage_download(ClientTransport &wire, const void *pixels, size_t bytes);
    void               finish_download(ClientTransport &wire, void *pixels, size_t bytes);
    void               release(ClientTransport &wire);
    void               forget();
    uint32_t           name() const;
    bool               invariants_hold() const;

private:
    enum class Staged : uint8_t { nothing, upload, download };

    [[nodiscard]] bool bind_storage(ClientTransport &wire, uint32_t target, size_t bytes, uint32_t usage);

    uint32_t name_;
    Staged   staged_;

    friend struct PixelStagingBufferProbe;
};

}

#endif
