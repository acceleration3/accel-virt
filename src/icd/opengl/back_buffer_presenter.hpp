#ifndef AV_GLICD_BACK_BUFFER_PRESENTER_HPP
#define AV_GLICD_BACK_BUFFER_PRESENTER_HPP

#include "icd/common/once_latch.hpp"
#include "core/log/driver_log.hpp"
#include "core/support/status.hpp"
#include "guest/transport/batch_mark.hpp"
#include "icd/common/handed_frame.hpp"
#include "icd/opengl/chained_frames.hpp"
#include "icd/opengl/front_buffer.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "proto/client/client_blob_arena.hpp"

#include "icd/common/arena_region.hpp"
#include <stddef.h>
#include <stdint.h>

namespace av {
class DriverLog;
}

namespace av {
class BlockSource;
class ClientTransport;
class StateWriter;
}

namespace av::icd {
class WindowSurface;
template <typename Source, typename Readiness>
class WindowPresentQueue;
}

namespace av::glicd {

class BackBufferPresenter;
class BatchReady;
class ClientArrayState;
struct BackBufferPresenterProbe;

using GlPresentQueue = icd::WindowPresentQueue<BackBufferPresenter, BatchReady>;
using GlFrame = icd::HandedFrame<BackBufferPresenter, BatchMark>;

class BackBufferPresenter {
public:
    static constexpr uint32_t max_edge = 8192u;
    static constexpr uint32_t slot_count = 2u;
    enum : uint32_t {
        gl_texture_2d = 0x0DE1u, gl_renderbuffer = 0x8D41u,
        gl_depth24_stencil8 = 0x88F0u, gl_depth_component32f = 0x8CACu, gl_color_attachment0 = 0x8CE0u,
        gl_depth_attachment = 0x8D00u, gl_depth_stencil_attachment = 0x821Au, gl_color_buffer_bit = 0x4000u,
        gl_nearest = 0x2600u, gl_bgra = 0x80E1u, gl_unsigned_byte = 0x1401u, gl_pixel_pack_buffer = 0x88EBu,
        gl_pack_row_length = 0x0D02u, gl_pack_skip_rows = 0x0D03u, gl_pack_skip_pixels = 0x0D04u,
        gl_pack_alignment = 0x0D05u, gl_pack_skip_images = 0x806Bu, gl_pack_image_height = 0x806Cu,
    };

    BackBufferPresenter(BlockSource &memory, DriverLog &log);
    ~BackBufferPresenter();
    BackBufferPresenter(const BackBufferPresenter &) = delete;
    BackBufferPresenter &operator=(const BackBufferPresenter &) = delete;

    [[nodiscard]] Status prepare(ClientTransport &wire, void *window, const PixelFormat &format,
                                 icd::WindowSurface &surface);
    void    *window() const;
    const FrontBuffer &front() const;
    [[nodiscard]] bool read_front(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                                  uint32_t type, void *pixels) const;
    uint32_t framebuffer() const;
    uint32_t width() const;
    uint32_t height() const;
    bool     double_buffered() const;
    bool     present(ClientTransport &wire, const ClientArrayState &state, void *dc, icd::WindowSurface &surface,
                     GlPresentQueue *loop, uint64_t luid);
    void     hand_over(const BatchMark &mark);
    void     done(const GlFrame &frame, bool drawn);
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
    void                 set_pack(ClientTransport &wire, const ClientArrayState &state, bool plain);
    void                 read_back(ClientTransport &wire);
    bool                 draw_chained(ClientTransport &wire, GlPresentQueue &loop, uint64_t luid);
    bool                 queue_read(ClientTransport &wire, const ClientArrayState &state, GlPresentQueue &loop);
    bool                 reserve_room(ClientTransport &wire, GlPresentQueue &loop);
    void                 give_room_back();
    uint8_t             *room_pixels(uint32_t slot) const;
    void                 settle() const;
    size_t               frame_bytes() const;
    size_t               stored_bytes() const;

    BlockSource  &memory_;
    DriverLog &lines_;
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
    GlPresentQueue  *loop_;
    icd::ArenaRegion room_;
    uint32_t         pending_;
    bool             pending_chained_;
    ChainedFrames    chained_;
    bool             chain_refused_;

    friend struct BackBufferPresenterProbe;
};

}

#endif
