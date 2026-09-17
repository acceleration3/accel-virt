/* One window's back buffer for one context (research.md §6, data-model.md): a
 * host framebuffer object with colour and depth/stencil storage matching the
 * window's pixel format - sRGB, samples - at the window's client size, and the
 * guest pixels a swap reads it back into and puts on the window's DC. Every
 * host call names its objects directly, so nothing the application bound is
 * disturbed; the pack state that shapes a read-back is set aside for the read
 * and put back after it. */
#ifndef AV_GLICD_BACK_BUFFER_PRESENTER_HPP
#define AV_GLICD_BACK_BUFFER_PRESENTER_HPP

#include "core/support/status.hpp"
#include "icd/opengl/pixel_format.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
}

namespace av::icd {
class WindowSurface;
}  // namespace av::icd

namespace av::glicd {

class ClientArrayState;
struct BackBufferPresenterProbe;

class BackBufferPresenter {
public:
    static constexpr uint32_t max_edge = 8192u;
    /* The rows one read-back asks for: half the arena's default when bulk
     * data crosses there, and what a reply holds inline otherwise. */
    static constexpr size_t   blob_read_bytes = size_t{8} << 20u;
    static constexpr size_t   inline_read_bytes = size_t{48} << 10u;
    enum : uint32_t {
        gl_texture_2d = 0x0DE1u, gl_renderbuffer = 0x8D41u, gl_rgba8 = 0x8058u, gl_srgb8_alpha8 = 0x8C43u,
        gl_depth24_stencil8 = 0x88F0u, gl_depth_component32f = 0x8CACu, gl_color_attachment0 = 0x8CE0u,
        gl_depth_attachment = 0x8D00u, gl_depth_stencil_attachment = 0x821Au, gl_color_buffer_bit = 0x4000u,
        gl_nearest = 0x2600u, gl_bgra = 0x80E1u, gl_unsigned_byte = 0x1401u, gl_pixel_pack_buffer = 0x88EBu,
        gl_pack_row_length = 0x0D02u, gl_pack_skip_rows = 0x0D03u, gl_pack_skip_pixels = 0x0D04u,
        gl_pack_alignment = 0x0D05u, gl_pack_skip_images = 0x806Bu, gl_pack_image_height = 0x806Cu,
    };

    BackBufferPresenter(BlockSource &memory, icd::LogSink &log);
    ~BackBufferPresenter();
    BackBufferPresenter(const BackBufferPresenter &) = delete;
    BackBufferPresenter &operator=(const BackBufferPresenter &) = delete;

    /* The host framebuffer of `window` for `format` at the size `surface`
     * answers, made once and made again when that size changed.
     * invalid_parameter for a window that is gone or past max_edge;
     * no_memory when the pixels or a host name could not be had, with
     * nothing kept. */
    [[nodiscard]] Status prepare(ClientTransport &wire, void *window, const PixelFormat &format,
                                 icd::WindowSurface &surface);
    void    *window() const;
    /* 0 until prepared. */
    uint32_t framebuffer() const;
    uint32_t width() const;
    uint32_t height() const;
    bool     double_buffered() const;
    /* The frame onto `dc`: resolved, read back in rows the transport carries,
     * and put on the window bottom row first. false before a prepare; a DC
     * that refuses the frame is logged once and the swap still happened. */
    bool     present(ClientTransport &wire, const ClientArrayState &state, void *dc, icd::WindowSurface &surface);
    /* Deletes the host objects through `wire` when there is one - without one
     * they went with the host's context - and gives the pixels back. */
    void     release(ClientTransport *wire);

    void report(StateWriter &writer, uint32_t index) const;

    bool invariants_hold() const;

private:
    enum : uint32_t { draw = 0u, resolve = 1u, framebuffers = 2u };
    enum : uint32_t { colour = 0u, depth = 1u, renderbuffers = 2u };
    static constexpr uint32_t bytes_per_pixel = 4u;
    static constexpr int32_t  default_alignment = 4;

    [[nodiscard]] Status create(ClientTransport &wire);
    void                 delete_objects(ClientTransport &wire);
    void                 forget_objects();
    bool                 pack_is_plain(const ClientArrayState &state) const;
    /* The pack state a read-back needs, or the application's again. */
    void                 set_pack(ClientTransport &wire, const ClientArrayState &state, bool plain);
    void                 read_back(ClientTransport &wire);
    size_t               stored_bytes() const;

    BlockSource  &memory_;
    icd::LogSink &log_;
    void         *window_;
    PixelFormat   format_;
    uint32_t      width_;
    uint32_t      height_;
    uint32_t      framebuffers_[framebuffers];
    uint32_t      texture_;
    uint32_t      renderbuffers_[renderbuffers];
    uint8_t      *pixels_;
    size_t        capacity_;
    bool          refusal_logged_;

    friend struct BackBufferPresenterProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BACK_BUFFER_PRESENTER_HPP
