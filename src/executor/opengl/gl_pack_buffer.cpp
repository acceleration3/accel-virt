#include "executor/opengl/gl_pack_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"


namespace av {

namespace {

constexpr GLenum   pixel_pack_buffer = 0x88EBu;
constexpr GLenum   gpu_commands_complete = 0x9117u;
constexpr GLbitfield flush_commands = 0x1u;
constexpr GLuint64 most_wait_nanoseconds = 1000000000u;

}

GlPackBuffer::GlPackBuffer(const glproto::RealGl &gl, GlSharedBuffer &buffers) : gl_(gl), buffers_(buffers), segments_{}
{
    AV_SCOPE("GlPackBuffer::GlPackBuffer");
}

GlPackBuffer::~GlPackBuffer()
{
    AV_SCOPE("GlPackBuffer::~GlPackBuffer");
}

void GlPackBuffer::adopt(OwnedFd &descriptor, uint64_t base, uint64_t size)
{
    AV_SCOPE("GlPackBuffer::adopt");
    abandon(base);
    Segment &segment = segments_[base >> GuestBytes::slot_shift];
    segment.fd_.hold(descriptor.take());
    segment.base_ = base;
    segment.size_ = segment.fd_.held() ? size : 0u;
    segment.refused_ = false;
}

void GlPackBuffer::abandon(uint64_t base)
{
    AV_SCOPE("GlPackBuffer::abandon");
    if (base >> GuestBytes::slot_shift >= GuestBytes::slot_count) return;
    Segment &segment = segments_[base >> GuestBytes::slot_shift];
    segment.fd_.close();
    segment.size_ = 0u;
    segment.stale_ = segment.stale_ || segment.buffer_ != 0u || segment.memory_ != 0u;
}

void GlPackBuffer::abandon_all()
{
    AV_SCOPE("GlPackBuffer::abandon_all");
    for (uint32_t slot = 0u; slot < GuestBytes::slot_count; slot++) abandon(uint64_t{slot} << GuestBytes::slot_shift);
}

bool GlPackBuffer::bind(uint64_t at, uint64_t bytes, void *&offset)
{
    AV_SCOPE("GlPackBuffer::bind");
    offset = nullptr;
    Segment *const found = covering(at);
    if (found == nullptr) return false;
    Segment &segment = *found;
    if (segment.stale_) destroy(segment);
    if (!segment.fd_.held() || bytes == 0u) return false;
    const uint64_t inside = at - segment.base_;
    if (inside > segment.size_ || bytes > segment.size_ - inside) return false;
    if (segment.buffer_ == 0u && (segment.refused_ || !make(segment))) return false;
    gl_.glBindBuffer(pixel_pack_buffer, segment.buffer_);
    offset = reinterpret_cast<void *>(static_cast<uintptr_t>(inside));
    return true;
}

GlPackBuffer::Segment *GlPackBuffer::covering(uint64_t at)
{
    AV_SCOPE("GlPackBuffer::covering");
    uint64_t slot = at >> GuestBytes::slot_shift;
    if (slot >= GuestBytes::slot_count) return nullptr;
    for (;; slot--) {
        Segment &segment = segments_[slot];
        if (segment.size_ != 0u) return at - segment.base_ <= segment.size_ ? &segment : nullptr;
        if (slot == 0u) return nullptr;
    }
}

void GlPackBuffer::unbind()
{
    AV_SCOPE("GlPackBuffer::unbind");
    gl_.glBindBuffer(pixel_pack_buffer, 0u);
    GLsync const written = gl_.glFenceSync(gpu_commands_complete, 0u);
    if (written == nullptr) return;
    static_cast<void>(gl_.glClientWaitSync(written, flush_commands, most_wait_nanoseconds));
    gl_.glDeleteSync(written);
}

bool GlPackBuffer::packing() const
{
    AV_SCOPE("GlPackBuffer::packing");
    for (const Segment &segment : segments_)
        if (segment.buffer_ != 0u) return true;
    return false;
}

void GlPackBuffer::release()
{
    AV_SCOPE("GlPackBuffer::release");
    for (Segment &segment : segments_) destroy(segment);
}

void GlPackBuffer::destroy(Segment &segment)
{
    AV_SCOPE("GlPackBuffer::destroy");
    if (segment.buffer_ != 0u || segment.memory_ != 0u) buffers_.destroy(segment.memory_, segment.buffer_);
    segment.buffer_ = 0u;
    segment.memory_ = 0u;
    segment.stale_ = false;
}

bool GlPackBuffer::make(Segment &segment)
{
    AV_SCOPE("GlPackBuffer::make");
    segment.refused_ = gl_.glBindBuffer == nullptr || gl_.glFenceSync == nullptr || gl_.glClientWaitSync == nullptr ||
                       gl_.glDeleteSync == nullptr ||
                       !buffers_.make(segment.fd_.fd(), segment.size_, segment.memory_, segment.buffer_);
    return !segment.refused_;
}

}
