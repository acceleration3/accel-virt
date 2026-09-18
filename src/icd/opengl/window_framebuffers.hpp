/* The windows one context draws on (research.md §6): a BackBufferPresenter per
 * window it was made current on, the window it is current on now, and which of
 * the application's draw and read bindings is framebuffer 0 - the window. What
 * the application calls framebuffer 0, GL_BACK or GL_FRONT is translated to the
 * current window's framebuffer object and its colour attachment on the way to
 * the host, and back again in what a query answers. */
#ifndef AV_GLICD_WINDOW_FRAMEBUFFERS_HPP
#define AV_GLICD_WINDOW_FRAMEBUFFERS_HPP

#include "core/support/object_storage.hpp"
#include "core/support/status.hpp"
#include "icd/opengl/back_buffer_presenter.hpp"

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
class PixelFormat;
struct WindowFramebuffersProbe;

class WindowFramebuffers {
public:
    static constexpr uint32_t max_windows = 8u;
    enum : uint32_t {
        gl_none = 0u, gl_front_left = 0x0400u, gl_back_left = 0x0402u, gl_front = 0x0404u, gl_back = 0x0405u,
        gl_left = 0x0406u, gl_front_and_back = 0x0408u, gl_draw_buffer = 0x0C01u, gl_read_buffer = 0x0C02u,
        gl_draw_buffer0 = 0x8825u, gl_draw_framebuffer_binding = 0x8CA6u, gl_read_framebuffer = 0x8CA8u,
        gl_draw_framebuffer = 0x8CA9u, gl_read_framebuffer_binding = 0x8CAAu, gl_color_attachment0 = 0x8CE0u,
        gl_framebuffer = 0x8D40u,
    };

    WindowFramebuffers(BlockSource &memory, icd::LogSink &log);
    ~WindowFramebuffers();
    WindowFramebuffers(const WindowFramebuffers &) = delete;
    WindowFramebuffers &operator=(const WindowFramebuffers &) = delete;

    /* The context was made current on `window`: its presenter prepared, and,
     * the first time the context is current anywhere, the window bound for
     * drawing and reading with the viewport and scissor at its size, as a new
     * context finds them; afterwards a binding the application left on the
     * window follows it to this one. no_space past max_windows windows;
     * prepare's refusals otherwise, with the current window unchanged. */
    [[nodiscard]] Status make_current_on(ClientTransport &wire, void *window, const PixelFormat &format,
                                         icd::WindowSurface &surface);
    /* Null until the context is current on a window, or once that window is gone. */
    void    *window() const;
    /* An application's framebuffer name as the host knows it: 0 is the
     * current window's framebuffer object. */
    uint32_t name_of(uint32_t name) const;
    /* glBindFramebuffer(target, name), which of the two bindings is the window now. */
    void     bound(uint32_t target, uint32_t name);
    /* What the application last named as its read buffer, so a read of
     * GL_FRONT is answered from the frame the window is showing (feature
     * 051). */
    void     reading(uint32_t buffer);
    /* Answers `width` x `height` pixels from (`x`, `y`) out of the current
     * window's last presented frame; false unless the read is of GL_FRONT on
     * the window, and then the host answers it as before. */
    [[nodiscard]] bool read_front(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                                  uint32_t type, void *pixels) const;
    /* A draw or read buffer the application names, as the window's
     * framebuffer object takes it: the window's buffers are its colour
     * attachment while that binding is the window. */
    uint32_t buffer_for(uint32_t buffer, bool drawing) const;
    /* A glGetIntegerv answer as the application must see it. */
    int32_t  answer(uint32_t pname, int32_t host_value) const;
    /* The frame of `window` onto `dc` and, when the window's size changed, its
     * framebuffer made again - bound again where the application left the
     * window bound. false for a window this context never drew on. */
    bool     present(ClientTransport &wire, const ClientArrayState &state, void *dc, void *window,
                     const PixelFormat &format, icd::WindowSurface &surface);
    /* The presenters of windows that are gone, given up. */
    void     drop_gone(ClientTransport &wire, icd::WindowSurface &surface);
    /* Every presenter given up; the host's objects deleted through `wire` when there is one. */
    void     release(ClientTransport *wire);
    uint32_t held() const;

    void report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    uint32_t find(void *window) const;

    BlockSource                        &memory_;
    icd::LogSink                       &log_;
    ObjectStorage<BackBufferPresenter>  presenters_[max_windows];
    uint32_t                            current_;
    bool                                draws_window_;
    bool                                reads_window_;
    /* The buffer glReadBuffer last named, GL_BACK until it is named. */
    uint32_t                            read_buffer_;

    bool                                ever_current_;

    friend struct WindowFramebuffersProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_WINDOW_FRAMEBUFFERS_HPP
