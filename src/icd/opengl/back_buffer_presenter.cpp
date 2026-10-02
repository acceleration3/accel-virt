#include "icd/opengl/back_buffer_presenter.hpp"

#include "core/platform/block_source.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "core/log/log_sink.hpp"
#include "icd/common/row_pieces.hpp"
#include "icd/opengl/client_array_state.hpp"
#include "icd/common/window_surface.hpp"
#include "icd/opengl/gl_present_loop.hpp"
#include "proto/client/client_blob_arena.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/resources/texel_layout.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

namespace {


}  // namespace

static_assert(BackBufferPresenter::max_edge * 4u <= BackBufferPresenter::inline_read_bytes,
              "a read-back of one row fits a reply");

BackBufferPresenter::BackBufferPresenter(BlockSource &memory, LogSink &log)
    : memory_(memory), log_(log), lines_(log, "avgl: "), window_(nullptr), format_(0u, 0u, false, false, 0u), width_(0u), height_(0u),
      framebuffers_{}, texture_(0u), renderbuffers_{}, pixels_(nullptr), front_(), capacity_(0u), refusal_logged_(),
      loop_(nullptr), arena_(nullptr), room_(), pending_(slot_count), pending_chained_(false), chained_(),
      chain_refused_(false)
{
    AV_SCOPE("BackBufferPresenter::BackBufferPresenter");
}

BackBufferPresenter::~BackBufferPresenter()
{
    AV_SCOPE("BackBufferPresenter::~BackBufferPresenter");
    release(nullptr);
}

Status BackBufferPresenter::prepare(ClientTransport &wire, void *window, const PixelFormat &format,
                                    icd::WindowSurface &surface)
{
    AV_SCOPE("BackBufferPresenter::prepare");
    uint32_t width = 0u;
    uint32_t height = 0u;
    if (!surface.client_size(window, width, height) || width > max_edge || height > max_edge)
        return Status::invalid_parameter;
    if (framebuffers_[draw] != 0u && window == window_ && width == width_ && height == height_) return Status::ok;
    /* The thread may be showing a frame of the old size from the old room. */
    settle();
    delete_objects(wire);
    chain_refused_ = false;
    window_ = window;
    format_ = format;
    width_ = width;
    height_ = height;
    const size_t bytes = stored_bytes();
    if (bytes > capacity_) {
        front_.forget();
        if (pixels_ != nullptr) memory_.release(pixels_);
        pixels_ = static_cast<uint8_t *>(memory_.allocate(bytes, Pool::paged));
        capacity_ = pixels_ != nullptr ? bytes : 0u;
    }
    const Status made = pixels_ != nullptr ? create(wire) : Status::no_memory;
    if (made == Status::ok) return Status::ok;
    release(&wire);
    return made;
}

const FrontBuffer &BackBufferPresenter::front() const
{
    AV_SCOPE("BackBufferPresenter::front");
    return front_;
}

void    *BackBufferPresenter::window() const
{
    AV_SCOPE("BackBufferPresenter::window");
    return window_;
}
uint32_t BackBufferPresenter::framebuffer() const
{
    AV_SCOPE("BackBufferPresenter::framebuffer");
    return framebuffers_[draw];
}
uint32_t BackBufferPresenter::width() const
{
    AV_SCOPE("BackBufferPresenter::width");
    return width_;
}
uint32_t BackBufferPresenter::height() const
{
    AV_SCOPE("BackBufferPresenter::height");
    return height_;
}
bool     BackBufferPresenter::double_buffered() const
{
    AV_SCOPE("BackBufferPresenter::double_buffered");
    return format_.double_buffer();
}

bool BackBufferPresenter::present(ClientTransport &wire, const ClientArrayState &state, void *dc,
                                  icd::WindowSurface &surface, GlPresentLoop *loop, uint64_t luid)
{
    AV_SCOPE("BackBufferPresenter::present");
    if (framebuffers_[draw] == 0u) return false;
    if (width_ == 0u || height_ == 0u) return true;
    const auto right = static_cast<int32_t>(width_);
    const auto top = static_cast<int32_t>(height_);
    if (framebuffers_[resolve] != 0u)
        glproto::av_glBlitNamedFramebuffer(wire, framebuffers_[draw], framebuffers_[resolve], 0, 0, right, top, 0, 0,
                                            right, top, gl_color_buffer_bit, gl_nearest);
    if (loop != nullptr && draw_chained(wire, *loop, luid)) return true;
    if (loop != nullptr && queue_read(wire, state, *loop)) return true;
    /* Shown after whatever the thread still has of this window's. */
    settle();
    const bool plain = pack_is_plain(state);
    if (!plain) set_pack(wire, state, true);
    read_back(wire);
    if (!plain) set_pack(wire, state, false);
    /* What the window is showing now is what a read of GL_FRONT answers. */
    front_.presented(pixels_, width_, height_);
    if (surface.blit(dc, width_, height_, pixels_) || !refusal_logged_.first()) return true;
    LogLine(lines_).text("the window's DC refused a frame of ").number(width_).text("x").number(height_).write();
    return true;
}

void BackBufferPresenter::hand_over(const BatchMark &mark)
{
    AV_SCOPE("BackBufferPresenter::hand_over");
    if (pending_ == slot_count || loop_ == nullptr) return;
    const HandedFrame frame(*this, pending_, pending_chained_ ? nullptr : room_pixels(pending_), width_, height_,
                            window_, mark);
    pending_ = slot_count;
    pending_chained_ = false;
    /* A frame the table has no row for is not shown; the next one will be. */
    static_cast<void>(loop_->hand(frame));
}

void BackBufferPresenter::shown(const uint8_t *pixels, uint32_t width, uint32_t height)
{
    AV_SCOPE("BackBufferPresenter::shown");
    const size_t bytes = static_cast<size_t>(width) * height * bytes_per_pixel;
    if (pixels_ == nullptr || bytes > capacity_) return;
    __builtin_memcpy(pixels_, pixels, bytes);
    front_.presented(pixels_, width, height);
}

bool BackBufferPresenter::read_front(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t format,
                                     uint32_t type, void *pixels) const
{
    AV_SCOPE("BackBufferPresenter::read_front");
    /* A chained frame never crossed into guest memory: the host answers a read
     * of the front buffer from the window's framebuffer instead. */
    if (chained_.opened()) return false;
    if (loop_ != nullptr) loop_->acquire();
    const bool read = front_.read(x, y, width, height, format, type, pixels);
    if (loop_ != nullptr) loop_->release();
    return read;
}

void BackBufferPresenter::release(ClientTransport *wire)
{
    AV_SCOPE("BackBufferPresenter::release");
    settle();
    give_room_back();
    pending_ = slot_count;
    pending_chained_ = false;
    if (loop_ != nullptr) chained_.close(wire, loop_->chains());
    loop_ = nullptr;
    if (wire != nullptr) delete_objects(*wire);
    forget_objects();
    front_.forget();
    if (pixels_ != nullptr) memory_.release(pixels_);
    pixels_ = nullptr;
    capacity_ = 0u;
}

void BackBufferPresenter::report(StateWriter &writer, uint32_t index) const
{
    AV_SCOPE("BackBufferPresenter::report");
    writer.open("presenter", index);
    writer.hex("hwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window_)));
    writer.number("framebuffer", framebuffers_[draw]);
    writer.number("resolve", framebuffers_[resolve]);
    writer.number("width", width_);
    writer.number("height", height_);
    writer.number("samples", format_.samples());
    writer.flag("srgb", format_.srgb());
    writer.number("room", room_.bytes());
    writer.number("pending", pending_);
    writer.flag("chained", chained_.opened());
    writer.close();
}

/* A made presenter has its framebuffer, colour texture and the pixels its
 * size needs; one that is not holds no host name. */
bool BackBufferPresenter::invariants_hold() const
{
    AV_SCOPE("BackBufferPresenter::invariants_hold");
    if (pending_ > slot_count || room_.taken() != (arena_ != nullptr)) return false;
    if (pending_ != slot_count && !room_.taken() && !pending_chained_) return false;
    if (pending_chained_ && (pending_ == slot_count || !chained_.opened())) return false;
    if (!chained_.invariants_hold()) return false;
    const bool multisampled = format_.samples() != 0u;
    const bool depth_stencil = format_.depth_bits() != 0u;
    if (framebuffers_[draw] == 0u)
        return framebuffers_[resolve] == 0u && texture_ == 0u && renderbuffers_[colour] == 0u &&
               renderbuffers_[depth] == 0u;
    if (texture_ == 0u || pixels_ == nullptr || capacity_ < stored_bytes()) return false;
    if (multisampled != (framebuffers_[resolve] != 0u && renderbuffers_[colour] != 0u)) return false;
    return depth_stencil == (renderbuffers_[depth] != 0u);
}

Status BackBufferPresenter::create(ClientTransport &wire)
{
    AV_SCOPE("BackBufferPresenter::create");
    const bool     multisampled = format_.samples() != 0u;
    const bool     depth_stencil = format_.depth_bits() != 0u;
    const uint32_t made_framebuffers = multisampled ? 2u : 1u;
    glproto::av_glCreateFramebuffers(wire, static_cast<GLsizei>(made_framebuffers), framebuffers_);
    glproto::av_glCreateTextures(wire, gl_texture_2d, 1, &texture_);
    /* The colour renderbuffer first, when there is one; the depth one after. */
    uint32_t made[renderbuffers] = {};
    const uint32_t made_renderbuffers = (multisampled ? 1u : 0u) + (depth_stencil ? 1u : 0u);
    if (made_renderbuffers != 0u)
        glproto::av_glCreateRenderbuffers(wire, static_cast<GLsizei>(made_renderbuffers), made);
    renderbuffers_[colour] = multisampled ? made[0] : 0u;
    renderbuffers_[depth] = depth_stencil ? made[made_renderbuffers - 1u] : 0u;
    /* No framebuffer at all is the one incomplete set the invariant allows. */
    if (framebuffers_[draw] == 0u || !invariants_hold()) return Status::no_memory;
    const auto     edge_width = static_cast<GLsizei>(width_ != 0u ? width_ : 1u);
    const auto     edge_height = static_cast<GLsizei>(height_ != 0u ? height_ : 1u);
    const auto     samples = static_cast<GLsizei>(format_.samples());
    const uint32_t colour_format = format_.srgb() ? gl_srgb8_alpha8 : gl_rgba8;
    glproto::av_glTextureStorage2D(wire, texture_, 1, colour_format, edge_width, edge_height);
    const uint32_t sampled = multisampled ? framebuffers_[resolve] : framebuffers_[draw];
    glproto::av_glNamedFramebufferTexture(wire, sampled, gl_color_attachment0, texture_, 0);
    if (multisampled) {
        glproto::av_glNamedRenderbufferStorageMultisample(wire, renderbuffers_[colour], samples, colour_format,
                                                           edge_width, edge_height);
        glproto::av_glNamedFramebufferRenderbuffer(wire, framebuffers_[draw], gl_color_attachment0, gl_renderbuffer,
                                                    renderbuffers_[colour]);
    }
    if (depth_stencil) {
        const bool stencil = format_.stencil_bits() != 0u;
        glproto::av_glNamedRenderbufferStorageMultisample(wire, renderbuffers_[depth], samples,
                                                           stencil ? gl_depth24_stencil8 : gl_depth_component32f,
                                                           edge_width, edge_height);
        glproto::av_glNamedFramebufferRenderbuffer(wire, framebuffers_[draw],
                                                    stencil ? gl_depth_stencil_attachment : gl_depth_attachment,
                                                    gl_renderbuffer, renderbuffers_[depth]);
    }
    return Status::ok;
}

/* OpenGL deletes nothing for a zero name, so a partly made set is sent as it is. */
void BackBufferPresenter::delete_objects(ClientTransport &wire)
{
    AV_SCOPE("BackBufferPresenter::delete_objects");
    const uint32_t names[] = {framebuffers_[draw], framebuffers_[resolve], texture_, renderbuffers_[colour],
                              renderbuffers_[depth]};
    bool           held = false;
    for (const uint32_t name : names) held = held || name != 0u;
    if (held) {
        glproto::av_glDeleteFramebuffers(wire, framebuffers, framebuffers_);
        glproto::av_glDeleteTextures(wire, 1, &texture_);
        glproto::av_glDeleteRenderbuffers(wire, renderbuffers, renderbuffers_);
    }
    forget_objects();
}

void BackBufferPresenter::forget_objects()
{
    AV_SCOPE("BackBufferPresenter::forget_objects");
    framebuffers_[draw] = 0u;
    framebuffers_[resolve] = 0u;
    texture_ = 0u;
    renderbuffers_[colour] = 0u;
    renderbuffers_[depth] = 0u;
}

/* Rows of four bytes a pixel, with no pack buffer to write them into instead:
 * what the application's pack state gives, or what it has to be set to. */
bool BackBufferPresenter::pack_is_plain(const ClientArrayState &state) const
{
    AV_SCOPE("BackBufferPresenter::pack_is_plain");
    if (state.pixel_buffer_bound(true)) return false;
    const size_t whole = static_cast<size_t>(width_) * height_ * bytes_per_pixel;
    return state.pixel_bytes(gl_bgra, gl_unsigned_byte, static_cast<int32_t>(width_), static_cast<int32_t>(height_),
                             1, true) == whole;
}

void BackBufferPresenter::set_pack(ClientTransport &wire, const ClientArrayState &state, bool plain)
{
    AV_SCOPE("BackBufferPresenter::set_pack");
    constexpr uint32_t names[] = {gl_pack_alignment,   gl_pack_row_length, gl_pack_skip_rows,
                                  gl_pack_skip_pixels, gl_pack_skip_images, gl_pack_image_height};
    glproto::av_glBindBuffer(wire, gl_pixel_pack_buffer, plain ? 0u : state.bound_buffer(gl_pixel_pack_buffer));
    for (const uint32_t name : names) {
        const int32_t neutral = name == gl_pack_alignment ? default_alignment : 0;
        glproto::av_glPixelStorei(wire, name, plain ? neutral : state.pixel_value(name));
    }
}

void BackBufferPresenter::read_back(ClientTransport &wire)
{
    AV_SCOPE("BackBufferPresenter::read_back");
    const TexelLayout pixels(bytes_per_pixel, 1u, 1u);
    const size_t      chunk = wire.blobs().ready() ? blob_read_bytes : inline_read_bytes;
    icd::RowPieces    pieces(pixels, width_, height_, width_ * bytes_per_pixel, chunk);
    while (pieces.next())
        glproto::av_glGetTextureSubImage(wire, texture_, 0, static_cast<GLint>(pieces.left()),
                                          static_cast<GLint>(pieces.top()), 0,
                                          static_cast<GLsizei>(pieces.right() - pieces.left()),
                                          static_cast<GLsizei>(pieces.bottom() - pieces.top()), 1, gl_bgra,
                                          gl_unsigned_byte, static_cast<GLsizei>(pieces.bytes()),
                                          pixels_ + pieces.offset());
}

/* Opened at the window's size the first time a frame is drawn, and again
 * after it changed; refused once, the window's frames are read back until its
 * size changes. The thread holds no frame of this presenter's while its chain
 * is opened. */
bool BackBufferPresenter::draw_chained(ClientTransport &wire, GlPresentLoop &loop, uint64_t luid)
{
    AV_SCOPE("BackBufferPresenter::draw_chained");
    if (chain_refused_ || luid == 0u || !loop.ready()) return false;
    loop_ = &loop;
    if (!chained_.fits(window_, width_, height_)) {
        settle();
        if (!chained_.open(wire, loop.chains(), window_, luid, width_, height_, format_.srgb())) {
            chain_refused_ = true;
            log_.write("avgl: a window's frames cannot be presented as host surfaces; they are read back");
            return false;
        }
    }
    const uint32_t slot = loop.free_slot(*this, ChainedFrames::slot_count, pending_);
    /* Nothing came free: the thread is stuck, and the frame is dropped. */
    if (slot == ChainedFrames::slot_count) return true;
    /* The colour texture holds the frame, resolved when the format is
     * multisampled. */
    chained_.draw(wire, texture_, slot);
    pending_ = slot;
    pending_chained_ = true;
    return true;
}

/* The present thread cannot have a frame of this presenter's before a present
 * reads one into a slot, and one read is always handed over before the next
 * present: so a free slot is one the thread is not holding. */
bool BackBufferPresenter::queue_read(ClientTransport &wire, const ClientArrayState &state, GlPresentLoop &loop)
{
    AV_SCOPE("BackBufferPresenter::queue_read");
    if (!loop.ready() || !reserve_room(wire, loop)) return false;
    const uint32_t slot = loop.free_slot(*this, slot_count, pending_);
    /* Nothing came free: the thread is stuck, and the frame is dropped. */
    if (slot == slot_count) return true;
    const bool plain = pack_is_plain(state);
    if (!plain) set_pack(wire, state, true);
    const size_t frame = frame_bytes();
    glproto::av_glGetTextureSubImage_into_blob(wire, texture_, 0, 0, 0, 0, static_cast<GLsizei>(width_),
                                                static_cast<GLsizei>(height_), 1, gl_bgra, gl_unsigned_byte,
                                                static_cast<GLsizei>(frame), room_.offset() + slot * frame);
    if (!plain) set_pack(wire, state, false);
    pending_ = slot;
    return true;
}

bool BackBufferPresenter::reserve_room(ClientTransport &wire, GlPresentLoop &loop)
{
    AV_SCOPE("BackBufferPresenter::reserve_room");
    const size_t wanted = frame_bytes() * slot_count;
    if (room_.taken() && room_.bytes() == wanted) return true;
    loop_ = &loop;
    settle();
    give_room_back();
    ClientBlobArena &arena = wire.blobs();
    room_ = arena.set_aside(wanted);
    if (!room_.taken()) return false;
    arena_ = &arena;
    return true;
}

/* Only the most recent room goes back to lending; an older one is lent again
 * when the arena closes with the context. */
void BackBufferPresenter::give_room_back()
{
    AV_SCOPE("BackBufferPresenter::give_room_back");
    if (arena_ != nullptr) arena_->restore(room_);
    arena_ = nullptr;
    room_ = ClientBlobLease();
}

void BackBufferPresenter::settle() const
{
    AV_SCOPE("BackBufferPresenter::settle");
    if (loop_ != nullptr) loop_->settle(*this);
}

uint8_t *BackBufferPresenter::room_pixels(uint32_t slot) const
{
    AV_SCOPE("BackBufferPresenter::room_pixels");
    return static_cast<uint8_t *>(room_.data()) + slot * frame_bytes();
}

size_t BackBufferPresenter::frame_bytes() const
{
    AV_SCOPE("BackBufferPresenter::frame_bytes");
    return static_cast<size_t>(width_) * height_ * bytes_per_pixel;
}

/* Never zero, so a 0x0 window still has storage the framebuffer can be complete with. */
size_t BackBufferPresenter::stored_bytes() const
{
    AV_SCOPE("BackBufferPresenter::stored_bytes");
    const size_t width = width_ != 0u ? width_ : 1u;
    const size_t height = height_ != 0u ? height_ : 1u;
    return width * height * bytes_per_pixel;
}

}  // namespace av::glicd
