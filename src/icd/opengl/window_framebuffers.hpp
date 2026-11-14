#ifndef AV_GLICD_WINDOW_FRAMEBUFFERS_HPP
#define AV_GLICD_WINDOW_FRAMEBUFFERS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"

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
}

namespace av::icd {
class PresentPacing;
class WindowSurface;
}

namespace av::glicd {

class ClientArrayState;
class PixelFormat;
struct WindowFramebuffersProbe;

class WindowFramebuffers {
public:
    enum : uint32_t {
        gl_none = 0u, gl_front_left = 0x0400u, gl_back_left = 0x0402u, gl_front = 0x0404u, gl_back = 0x0405u,
        gl_left = 0x0406u, gl_front_and_back = 0x0408u, gl_draw_buffer = 0x0C01u, gl_read_buffer = 0x0C02u,
        gl_draw_buffer0 = 0x8825u, gl_draw_framebuffer_binding = 0x8CA6u, gl_read_framebuffer = 0x8CA8u,
        gl_draw_framebuffer = 0x8CA9u, gl_read_framebuffer_binding = 0x8CAAu, gl_color_attachment0 = 0x8CE0u,
        gl_framebuffer = 0x8D40u,
    };

    WindowFramebuffers(BlockSource &memory, DriverLog &log);
    ~WindowFramebuffers();
    WindowFramebuffers(const WindowFramebuffers &) = delete;
    WindowFramebuffers &operator=(const WindowFramebuffers &) = delete;

    [[nodiscard]] Status make_current_on(ClientTransport &wire, void *window, const PixelFormat &format,
                                         icd::WindowSurface &surface);
    void    *window() const;
    uint32_t name_of(uint32_t name) const;
    void     bound(uint32_t target, uint32_t name);
    void     reading(uint32_t buffer);
    [[nodiscard]] bool read_front(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                                  uint32_t type, void *pixels) const;
    uint32_t buffer_for(uint32_t buffer, bool drawing) const;
    int32_t  answer(uint32_t pname, int32_t host_value) const;
    bool     present(ClientTransport &wire, const ClientArrayState &state, void *dc, void *window,
                     icd::WindowSurface &surface, GlPresentQueue *loop, uint64_t luid);
    void     hand_over(const BatchMark &mark);
    void     hold_over(const BatchMark &mark, icd::PresentPacing &pacing);
    void     show_held_now();
    bool     viewport_resizes(uint32_t width, uint32_t height);
    void     follow(ClientTransport &wire, void *window, const PixelFormat &format, icd::WindowSurface &surface);
    void     drop_gone(ClientTransport &wire, icd::WindowSurface &surface);
    void     release(ClientTransport *wire);
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    BackBufferPresenter *find(void *window) const;
    uint64_t             key_of(void *window) const;
    void                 forget(BackBufferPresenter *presenter);
    void     bind_window(ClientTransport &wire, uint32_t framebuffer);

    BlockSource                        &memory_;
    DriverLog                     &log_;
    ObjectPool<BackBufferPresenter>     pool_;
    KeyedSlots<uint64_t, BackBufferPresenter *> presenters_;
    BackBufferPresenter                *current_;
    BackBufferPresenter                *presented_;
    bool                                draws_window_;
    bool                                reads_window_;
    uint32_t                            read_buffer_;
    uint32_t                            viewport_width_;
    uint32_t                            viewport_height_;

    bool                                ever_current_;

    friend struct WindowFramebuffersProbe;
};

}

#endif
