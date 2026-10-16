#include "executor/opengl/gl_pack_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <unistd.h>

namespace av {

namespace {

constexpr GLenum   pixel_pack_buffer = 0x88EBu;
constexpr GLenum   gpu_commands_complete = 0x9117u;
constexpr GLbitfield flush_commands = 0x1u;
/* A read-back not written in a second is a GPU that is gone. */
constexpr GLuint64 most_wait_nanoseconds = 1000000000u;

}  // namespace

GlPackBuffer::GlPackBuffer(const glproto::RealGl &gl, GlSharedBuffer &buffers)
    : gl_(gl), buffers_(buffers), fd_(-1), size_(0u), memory_(0u), buffer_(0u), refused_(false), stale_(false)
{
    AV_SCOPE("GlPackBuffer::GlPackBuffer");
}

GlPackBuffer::~GlPackBuffer()
{
    AV_SCOPE("GlPackBuffer::~GlPackBuffer");
    if (fd_ >= 0) close(fd_);
}

void GlPackBuffer::adopt(int fd, uint64_t size)
{
    AV_SCOPE("GlPackBuffer::adopt");
    abandon();
    fd_ = fd;
    size_ = fd_ >= 0 ? size : 0u;
    refused_ = false;
}

void GlPackBuffer::abandon()
{
    AV_SCOPE("GlPackBuffer::abandon");
    if (fd_ >= 0) close(fd_);
    fd_ = -1;
    size_ = 0u;
    stale_ = stale_ || buffer_ != 0u || memory_ != 0u;
}

bool GlPackBuffer::bind(uint64_t at, uint64_t bytes, void *&offset)
{
    AV_SCOPE("GlPackBuffer::bind");
    offset = nullptr;
    if (stale_) release();
    if (fd_ < 0 || bytes == 0u || at > size_ || bytes > size_ - at) return false;
    if (buffer_ == 0u && (refused_ || !make())) return false;
    gl_.glBindBuffer(pixel_pack_buffer, buffer_);
    offset = reinterpret_cast<void *>(static_cast<uintptr_t>(at));
    return true;
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
    return buffer_ != 0u;
}

void GlPackBuffer::release()
{
    AV_SCOPE("GlPackBuffer::release");
    if (buffer_ != 0u || memory_ != 0u) buffers_.destroy(memory_, buffer_);
    buffer_ = 0u;
    memory_ = 0u;
    stale_ = false;
}

/* A driver that will not make one is not asked again for this region. The
 * transfer itself is the table's: a context without what binding, fencing and
 * waiting take packs nothing. */
bool GlPackBuffer::make()
{
    AV_SCOPE("GlPackBuffer::make");
    refused_ = gl_.glBindBuffer == nullptr || gl_.glFenceSync == nullptr || gl_.glClientWaitSync == nullptr ||
               gl_.glDeleteSync == nullptr || !buffers_.make(fd_, size_, memory_, buffer_);
    return !refused_;
}

}  // namespace av
