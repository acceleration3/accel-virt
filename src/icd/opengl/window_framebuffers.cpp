#include "icd/opengl/window_framebuffers.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "icd/common/window_surface.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

WindowFramebuffers::WindowFramebuffers(BlockSource &memory, icd::LogSink &log)
    : memory_(memory), log_(log), presenters_{}, current_(max_windows), draws_window_(false), reads_window_(false),
      ever_current_(false)
{
    AV_SCOPE("WindowFramebuffers::WindowFramebuffers");
}

WindowFramebuffers::~WindowFramebuffers()
{
    AV_SCOPE("WindowFramebuffers::~WindowFramebuffers");
    release(nullptr);
}

Status WindowFramebuffers::make_current_on(ClientTransport &wire, void *window, const PixelFormat &format,
                                           icd::WindowSurface &surface)
{
    AV_SCOPE("WindowFramebuffers::make_current_on");
    uint32_t   slot = find(window);
    const bool added = slot == max_windows;
    if (added) {
        slot = find(nullptr);
        if (slot == max_windows) return Status::no_space;
        static_cast<void>(presenters_[slot].construct(memory_, log_));
    }
    BackBufferPresenter &presenter = presenters_[slot].object();
    const Status         prepared = presenter.prepare(wire, window, format, surface);
    if (prepared != Status::ok) {
        if (added) presenters_[slot].destroy();
        return prepared;
    }
    const bool moved = slot != current_;
    current_ = slot;
    if (!ever_current_) {
        ever_current_ = true;
        draws_window_ = true;
        reads_window_ = true;
        const auto width = static_cast<GLsizei>(presenter.width());
        const auto height = static_cast<GLsizei>(presenter.height());
        glproto::rvk_glBindFramebuffer(wire, gl_framebuffer, presenter.framebuffer());
        glproto::rvk_glViewport(wire, 0, 0, width, height);
        glproto::rvk_glScissor(wire, 0, 0, width, height);
        return Status::ok;
    }
    if (moved && draws_window_) glproto::rvk_glBindFramebuffer(wire, gl_draw_framebuffer, presenter.framebuffer());
    if (moved && reads_window_) glproto::rvk_glBindFramebuffer(wire, gl_read_framebuffer, presenter.framebuffer());
    return Status::ok;
}

void *WindowFramebuffers::window() const
{
    AV_SCOPE("WindowFramebuffers::window");
    return current_ != max_windows ? presenters_[current_].object().window() : nullptr;
}

uint32_t WindowFramebuffers::name_of(uint32_t name) const
{
    AV_SCOPE("WindowFramebuffers::name_of");
    if (name != 0u || current_ == max_windows) return name;
    return presenters_[current_].object().framebuffer();
}

void WindowFramebuffers::bound(uint32_t target, uint32_t name)
{
    AV_SCOPE("WindowFramebuffers::bound");
    const bool window = name == 0u;
    if (target == gl_framebuffer || target == gl_draw_framebuffer) draws_window_ = window;
    if (target == gl_framebuffer || target == gl_read_framebuffer) reads_window_ = window;
}

uint32_t WindowFramebuffers::buffer_for(uint32_t buffer, bool drawing) const
{
    AV_SCOPE("WindowFramebuffers::buffer_for");
    if (!(drawing ? draws_window_ : reads_window_)) return buffer;
    switch (buffer) {
    case gl_front_left: case gl_back_left: case gl_front: case gl_back: case gl_left: case gl_front_and_back:
        return gl_color_attachment0;
    default:
        return buffer;
    }
}

int32_t WindowFramebuffers::answer(uint32_t pname, int32_t host_value) const
{
    AV_SCOPE("WindowFramebuffers::answer");
    if (pname == gl_draw_framebuffer_binding) return draws_window_ ? 0 : host_value;
    if (pname == gl_read_framebuffer_binding) return reads_window_ ? 0 : host_value;
    const bool reading = pname == gl_read_buffer;
    if (!reading && pname != gl_draw_buffer && pname != gl_draw_buffer0) return host_value;
    const bool on_window = reading ? reads_window_ : draws_window_;
    if (!on_window || current_ == max_windows || host_value != static_cast<int32_t>(gl_color_attachment0))
        return host_value;
    const bool doubled = presenters_[current_].object().double_buffered();
    return static_cast<int32_t>(doubled ? gl_back : gl_front);
}

bool WindowFramebuffers::present(ClientTransport &wire, const ClientArrayState &state, void *dc, void *window,
                                 const PixelFormat &format, icd::WindowSurface &surface)
{
    AV_SCOPE("WindowFramebuffers::present");
    const uint32_t slot = find(window);
    if (window == nullptr || slot == max_windows) return false;
    BackBufferPresenter &presenter = presenters_[slot].object();
    const bool          presented = presenter.present(wire, state, dc, surface);
    const uint32_t      before = presenter.framebuffer();
    /* A window that shrank or grew gets its new size before the next frame;
     * one that cannot have it keeps the old until the window is gone. */
    static_cast<void>(presenter.prepare(wire, window, format, surface));
    const uint32_t after = presenter.framebuffer();
    if (after != before && slot == current_ && draws_window_) glproto::rvk_glBindFramebuffer(wire, gl_draw_framebuffer, after);
    if (after != before && slot == current_ && reads_window_) glproto::rvk_glBindFramebuffer(wire, gl_read_framebuffer, after);
    return presented;
}

void WindowFramebuffers::drop_gone(ClientTransport &wire, icd::WindowSurface &surface)
{
    AV_SCOPE("WindowFramebuffers::drop_gone");
    for (uint32_t slot = 0u; slot < max_windows; slot++) {
        if (!presenters_[slot].constructed()) continue;
        uint32_t width = 0u;
        uint32_t height = 0u;
        if (surface.client_size(presenters_[slot].object().window(), width, height)) continue;
        presenters_[slot].object().release(&wire);
        presenters_[slot].destroy();
        if (slot == current_) current_ = max_windows;
    }
}

void WindowFramebuffers::release(ClientTransport *wire)
{
    AV_SCOPE("WindowFramebuffers::release");
    for (ObjectStorage<BackBufferPresenter> &presenter : presenters_) {
        if (presenter.constructed()) presenter.object().release(wire);
        presenter.destroy();
    }
    current_ = max_windows;
}

uint32_t WindowFramebuffers::held() const
{
    AV_SCOPE("WindowFramebuffers::held");
    uint32_t count = 0u;
    for (const ObjectStorage<BackBufferPresenter> &presenter : presenters_) count += presenter.constructed() ? 1u : 0u;
    return count;
}

void WindowFramebuffers::report(StateWriter &writer) const
{
    AV_SCOPE("WindowFramebuffers::report");
    writer.open("windows", 0u);
    writer.number("held", held());
    writer.flag("draws_window", draws_window_);
    writer.flag("reads_window", reads_window_);
    for (uint32_t slot = 0u; slot < max_windows; slot++) {
        if (!presenters_[slot].constructed()) continue;
        presenters_[slot].object().report(writer, slot);
    }
    writer.close();
}

/* The current window is a held presenter, no window is held twice, and each
 * presenter is whole. */
bool WindowFramebuffers::invariants_hold() const
{
    AV_SCOPE("WindowFramebuffers::invariants_hold");
    if (current_ > max_windows || (current_ != max_windows && !presenters_[current_].constructed())) return false;
    for (uint32_t slot = 0u; slot < max_windows; slot++) {
        if (!presenters_[slot].constructed()) continue;
        const BackBufferPresenter &presenter = presenters_[slot].object();
        if (!presenter.invariants_hold()) return false;
        for (uint32_t earlier = 0u; earlier < slot; earlier++)
            if (presenters_[earlier].constructed() && presenters_[earlier].object().window() == presenter.window())
                return false;
    }
    return true;
}

/* The slot holding `window`; for null, the first free slot. */
uint32_t WindowFramebuffers::find(void *window) const
{
    AV_SCOPE("WindowFramebuffers::find");
    for (uint32_t slot = 0u; slot < max_windows; slot++) {
        const bool held = presenters_[slot].constructed();
        if (window == nullptr ? !held : held && presenters_[slot].object().window() == window) return slot;
    }
    return max_windows;
}

}  // namespace av::glicd
