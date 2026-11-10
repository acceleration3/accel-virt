#include "icd/opengl/window_framebuffers.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/opengl/pixel_format.hpp"
#include "icd/common/window_extent.hpp"
#include "icd/common/window_surface.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

WindowFramebuffers::WindowFramebuffers(BlockSource &memory, DriverLog &log)
    : memory_(memory), log_(log), pool_(memory), presenters_(memory, Pool::paged), current_(nullptr),
      presented_(nullptr), draws_window_(false),
      reads_window_(false),
      read_buffer_(gl_back), viewport_width_(0u), viewport_height_(0u), ever_current_(false)
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
    BackBufferPresenter *found = find(window);
    const bool           added = found == nullptr;
    if (added && (presenters_.reserve(presenters_.count() + 1u) != Status::ok ||
                  pool_.create(found, memory_, log_) != Status::ok))
        return Status::no_memory;
    BackBufferPresenter &presenter = *found;
    const uint32_t       before = presenter.framebuffer();
    const uint32_t       old_width = presenter.width();
    const uint32_t       old_height = presenter.height();
    const Status         prepared = presenter.prepare(wire, window, format, surface);
    if (prepared != Status::ok) {
        if (added) static_cast<void>(pool_.destroy(found));
        return prepared;
    }
    uint32_t slot = 0u;
    if (added) static_cast<void>(presenters_.insert(key_of(window), found, slot));
    const bool remade = !added && (presenter.framebuffer() != before || presenter.width() != old_width ||
                                   presenter.height() != old_height);
    const bool moved = found != current_;
    current_ = found;
    if (moved || remade) {
        viewport_width_ = 0u;
        viewport_height_ = 0u;
    }
    if (!ever_current_) {
        ever_current_ = true;
        draws_window_ = true;
        reads_window_ = true;
        const auto width = static_cast<GLsizei>(presenter.width());
        const auto height = static_cast<GLsizei>(presenter.height());
        glproto::av_glBindFramebuffer(wire, gl_framebuffer, presenter.framebuffer());
        glproto::av_glViewport(wire, 0, 0, width, height);
        glproto::av_glScissor(wire, 0, 0, width, height);
        return Status::ok;
    }
    if (moved || remade) bind_window(wire, presenter.framebuffer());
    return Status::ok;
}

void *WindowFramebuffers::window() const
{
    AV_SCOPE("WindowFramebuffers::window");
    return current_ != nullptr ? current_->window() : nullptr;
}

uint32_t WindowFramebuffers::name_of(uint32_t name) const
{
    AV_SCOPE("WindowFramebuffers::name_of");
    if (name != 0u || current_ == nullptr) return name;
    return current_->framebuffer();
}

void WindowFramebuffers::bound(uint32_t target, uint32_t name)
{
    AV_SCOPE("WindowFramebuffers::bound");
    const bool window = name == 0u;
    if (target == gl_framebuffer || target == gl_draw_framebuffer) draws_window_ = window;
    if (target == gl_framebuffer || target == gl_read_framebuffer) reads_window_ = window;
}

void WindowFramebuffers::reading(uint32_t buffer)
{
    AV_SCOPE("WindowFramebuffers::reading");
    read_buffer_ = buffer;
}

bool WindowFramebuffers::read_front(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                                    uint32_t type, void *pixels) const
{
    AV_SCOPE("WindowFramebuffers::read_front");
    if (!reads_window_ || current_ == nullptr) return false;
    if (read_buffer_ != gl_front && read_buffer_ != gl_front_left) return false;
    return current_->read_front(x, y, width, height, format, type, pixels);
}

bool WindowFramebuffers::reads_window() const
{
    AV_SCOPE("WindowFramebuffers::reads_window");
    return reads_window_;
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
    if (!on_window || current_ == nullptr || host_value != static_cast<int32_t>(gl_color_attachment0))
        return host_value;
    const bool doubled = current_->double_buffered();
    return static_cast<int32_t>(doubled ? gl_back : gl_front);
}

bool WindowFramebuffers::present(ClientTransport &wire, const ClientArrayState &state, void *dc, void *window,
                                 icd::WindowSurface &surface, GlPresentQueue *loop, uint64_t luid)
{
    AV_SCOPE("WindowFramebuffers::present");
    BackBufferPresenter *const presenter = find(window);
    if (presenter == nullptr) return false;
    presented_ = presenter;
    return presenter->present(wire, state, dc, surface, loop, luid);
}

void WindowFramebuffers::hand_over(const BatchMark &mark)
{
    AV_SCOPE("WindowFramebuffers::hand_over");
    if (presented_ != nullptr) presented_->hand_over(mark);
    presented_ = nullptr;
}

void WindowFramebuffers::hold_over(const BatchMark &mark, icd::PresentPacing &pacing)
{
    AV_SCOPE("WindowFramebuffers::hold_over");
    if (presented_ != nullptr) presented_->hold_over(mark, pacing);
    presented_ = nullptr;
}

bool WindowFramebuffers::viewport_resizes(uint32_t width, uint32_t height)
{
    AV_SCOPE("WindowFramebuffers::viewport_resizes");
    if (current_ == nullptr || (width == viewport_width_ && height == viewport_height_)) return false;
    viewport_width_ = width;
    viewport_height_ = height;
    return true;
}

void WindowFramebuffers::follow(ClientTransport &wire, void *window, const PixelFormat &format,
                                icd::WindowSurface &surface)
{
    AV_SCOPE("WindowFramebuffers::follow");
    BackBufferPresenter *const found = find(window);
    if (found == nullptr) return;
    BackBufferPresenter &presenter = *found;
    const uint32_t       before = presenter.framebuffer();
    const uint32_t       width = presenter.width();
    const uint32_t       height = presenter.height();
    static_cast<void>(presenter.prepare(wire, window, format, surface));
    const uint32_t after = presenter.framebuffer();
    const bool remade = after != before || presenter.width() != width || presenter.height() != height;
    if (remade && found == current_) bind_window(wire, after);
}

void WindowFramebuffers::drop_gone(ClientTransport &wire, icd::WindowSurface &surface)
{
    AV_SCOPE("WindowFramebuffers::drop_gone");
    presenters_.erase_if([this, &wire, &surface](uint64_t, BackBufferPresenter *&presenter) {
        if (icd::WindowExtent(surface, presenter->window()).look() != icd::WindowExtent::Look::gone) return false;
        presenter->release(&wire);
        forget(presenter);
        return true;
    });
}

void WindowFramebuffers::release(ClientTransport *wire)
{
    AV_SCOPE("WindowFramebuffers::release");
    presenters_.erase_if([this, wire](uint64_t, BackBufferPresenter *&presenter) {
        presenter->release(wire);
        forget(presenter);
        return true;
    });
}

void WindowFramebuffers::show_held_now()
{
    AV_SCOPE("WindowFramebuffers::show_held_now");
    for (uint32_t slot = presenters_.next_live(0u); slot != presenters_.no_slot; slot = presenters_.next_live(slot + 1u))
        (*presenters_.at(slot))->show_held_now();
}

uint32_t WindowFramebuffers::held() const
{
    AV_SCOPE("WindowFramebuffers::held");
    return presenters_.count();
}

void WindowFramebuffers::report(StateWriter &writer) const
{
    AV_SCOPE("WindowFramebuffers::report");
    writer.open("windows", 0u);
    writer.number("held", held());
    writer.flag("draws_window", draws_window_);
    writer.flag("reads_window", reads_window_);
    for (uint32_t slot = presenters_.next_live(0u); slot != presenters_.no_slot; slot = presenters_.next_live(slot + 1u))
        (*presenters_.at(slot))->report(writer, slot);
    writer.close();
}

bool WindowFramebuffers::invariants_hold() const
{
    AV_SCOPE("WindowFramebuffers::invariants_hold");
    bool whole = presenters_.count() == pool_.live();
    bool current_held = current_ == nullptr;
    bool presented_held = presented_ == nullptr;
    presenters_.for_each([&](uint64_t window, BackBufferPresenter *const &presenter) {
        whole = whole && presenter->invariants_hold() && key_of(presenter->window()) == window;
        current_held = current_held || presenter == current_;
        presented_held = presented_held || presenter == presented_;
    });
    return whole && current_held && presented_held && presenters_.invariants_hold();
}

void WindowFramebuffers::bind_window(ClientTransport &wire, uint32_t framebuffer)
{
    AV_SCOPE("WindowFramebuffers::bind_window");
    if (draws_window_) glproto::av_glBindFramebuffer(wire, gl_draw_framebuffer, framebuffer);
    if (reads_window_) glproto::av_glBindFramebuffer(wire, gl_read_framebuffer, framebuffer);
}

BackBufferPresenter *WindowFramebuffers::find(void *window) const
{
    AV_SCOPE("WindowFramebuffers::find");
    BackBufferPresenter *const *const presenter = window != nullptr ? presenters_.find(key_of(window)) : nullptr;
    return presenter != nullptr ? *presenter : nullptr;
}

uint64_t WindowFramebuffers::key_of(void *window) const
{
    AV_SCOPE("WindowFramebuffers::key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window));
}

void WindowFramebuffers::forget(BackBufferPresenter *presenter)
{
    AV_SCOPE("WindowFramebuffers::forget");
    if (presenter == current_) current_ = nullptr;
    if (presenter == presented_) presented_ = nullptr;
    static_cast<void>(pool_.destroy(presenter));
}

}
