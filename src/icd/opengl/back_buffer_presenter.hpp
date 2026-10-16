/* One window's back buffer for one context (research.md §6, data-model.md): a
 * host framebuffer object with colour and depth/stencil storage matching the
 * window's pixel format - sRGB, samples - at the window's client size, and the
 * guest pixels a swap reads it back into and puts on the window's DC. Every
 * host call names its objects directly, so nothing the application bound is
 * disturbed; the pack state that shapes a read-back is set aside for the read
 * and put back after it.
 *
 * With a present thread (GlPresentLoop) and room for two frames set aside in
 * the context's blob arena, a swap does not read back and blit: it sends the
 * read one-way into a free slot of that room and, once the swap's batch is
 * sent, hands the frame to the thread, which shows it when the host has
 * written it. The room changes, and the pixels the front buffer answers from
 * are written, only with nothing of this presenter's in the thread's hands. */
#ifndef AV_GLICD_BACK_BUFFER_PRESENTER_HPP
#define AV_GLICD_BACK_BUFFER_PRESENTER_HPP

#include "icd/common/once_latch.hpp"
#include "core/log/driver_log.hpp"
#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "icd/opengl/chained_frames.hpp"
#include "icd/opengl/front_buffer.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "proto/client/client_blob_arena.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}  // namespace av

namespace av::icd {
}

namespace av::icd {
class WindowSurface;
}  // namespace av::icd

namespace av::glicd {

class ClientArrayState;
class GlPresentLoop;
struct BackBufferPresenterProbe;

class BackBufferPresenter {
public:
    static constexpr uint32_t max_edge = 8192u;
    /* Frames the present thread may hold at once: one being shown while the
     * next is drawn, as a native driver queues. */
    static constexpr uint32_t slot_count = 2u;
    enum : uint32_t {
        gl_texture_2d = 0x0DE1u, gl_renderbuffer = 0x8D41u,
        gl_depth24_stencil8 = 0x88F0u, gl_depth_component32f = 0x8CACu, gl_color_attachment0 = 0x8CE0u,
        gl_depth_attachment = 0x8D00u, gl_depth_stencil_attachment = 0x821Au, gl_color_buffer_bit = 0x4000u,
        gl_nearest = 0x2600u, gl_bgra = 0x80E1u, gl_unsigned_byte = 0x1401u, gl_pixel_pack_buffer = 0x88EBu,
        gl_pack_row_length = 0x0D02u, gl_pack_skip_rows = 0x0D03u, gl_pack_skip_pixels = 0x0D04u,
        gl_pack_alignment = 0x0D05u, gl_pack_skip_images = 0x806Bu, gl_pack_image_height = 0x806Cu,
    };

    BackBufferPresenter(BlockSource &memory, LogSink &log);
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
    /* The last frame this put on the window, which is what GL_FRONT names
     * (feature 051): the host has no front buffer, since the window is a
     * framebuffer object there. */
    const FrontBuffer &front() const;
    /* A read of GL_FRONT, answered from the last frame shown (FrontBuffer::read)
     * while the present thread cannot be writing it. */
    [[nodiscard]] bool read_front(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                                  uint32_t type, void *pixels) const;
    /* 0 until prepared. */
    uint32_t framebuffer() const;
    uint32_t width() const;
    uint32_t height() const;
    bool     double_buffered() const;
    /* The frame onto `dc`: resolved, then drawn into a slot of the window's
     * chain on the adapter `luid` (ChainedFrames), or read one-way into a slot
     * for `loop` - either way hand_over gives the thread the frame - or, with
     * no loop, thread or room, read back in rows the transport carries and put
     * on the window bottom row first. false before a prepare; a DC that
     * refuses the frame is logged once and the swap still happened. */
    bool     present(ClientTransport &wire, const ClientArrayState &state, void *dc, icd::WindowSurface &surface,
                     GlPresentLoop *loop, uint64_t luid);
    /* The frame the last present read into a slot goes to the present thread,
     * to be shown once the batch `mark` names is done. Nothing without one. */
    void     hand_over(const BatchMark &mark);
    /* The present thread showed `width` x `height` pixels: they are what
     * GL_FRONT names now. Called with the thread's lock held. */
    void     shown(const uint8_t *pixels, uint32_t width, uint32_t height);
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
    /* The frame drawn into a free slot of the window's chain for `loop`, the
     * chain opened first; false when there is no thread or no chain. */
    bool                 draw_chained(ClientTransport &wire, GlPresentLoop &loop, uint64_t luid);
    /* The frame read one-way into a free slot for `loop`; false when there is
     * no thread or no room, and the swap shows the frame itself. */
    bool                 queue_read(ClientTransport &wire, const ClientArrayState &state, GlPresentLoop &loop);
    /* Room for slot_count frames of this size, set aside in the wire's arena
     * once nothing of this presenter's is in the thread's hands. */
    bool                 reserve_room(ClientTransport &wire, GlPresentLoop &loop);
    void                 give_room_back();
    uint8_t             *room_pixels(uint32_t slot) const;
    /* Nothing of this presenter's is in the present thread's hands. */
    void                 settle() const;
    size_t               frame_bytes() const;
    size_t               stored_bytes() const;

    BlockSource  &memory_;
    LogSink &log_;
    DriverLog lines_;
    void         *window_;
    PixelFormat   format_;
    uint32_t      width_;
    uint32_t      height_;
    uint32_t      framebuffers_[framebuffers];
    uint32_t      texture_;
    uint32_t      renderbuffers_[renderbuffers];
    uint8_t      *pixels_;
    FrontBuffer   front_;
    size_t        capacity_;
    icd::OnceLatch refusal_logged_;
    GlPresentLoop   *loop_;
    ClientBlobArena *arena_;
    ClientBlobLease  room_;
    /* The slot the last present read into, until handed over; slot_count
     * when none. */
    uint32_t         pending_;
    /* That slot is one of the chain's, not of the room. */
    bool             pending_chained_;
    ChainedFrames    chained_;
    /* The chain could not be had at this size: frames are read back until the
     * window's size changes. */
    bool             chain_refused_;

    friend struct BackBufferPresenterProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_BACK_BUFFER_PRESENTER_HPP
