#include "executor/opengl/gl_pack_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "executor/shim/gl_host_import.hpp"
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

GlPackBuffer::GlPackBuffer() : fd_(-1), size_(0u), memory_(0u), buffer_(0u), refused_(false), stale_(false)
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
    fd_ = fd >= 0 ? dup(fd) : -1;
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

bool GlPackBuffer::bind(const glproto::RealGl &gl, GlHostImport &import, uint64_t at, uint64_t bytes, void *&offset)
{
    AV_SCOPE("GlPackBuffer::bind");
    offset = nullptr;
    if (stale_) release(gl, import);
    if (fd_ < 0 || bytes == 0u || at > size_ || bytes > size_ - at) return false;
    if (buffer_ == 0u && (refused_ || !make(gl, import))) return false;
    gl.glBindBuffer(pixel_pack_buffer, buffer_);
    offset = reinterpret_cast<void *>(static_cast<uintptr_t>(at));
    return true;
}

void GlPackBuffer::unbind(const glproto::RealGl &gl)
{
    AV_SCOPE("GlPackBuffer::unbind");
    gl.glBindBuffer(pixel_pack_buffer, 0u);
    GLsync const written = gl.glFenceSync(gpu_commands_complete, 0u);
    if (written == nullptr) return;
    static_cast<void>(gl.glClientWaitSync(written, flush_commands, most_wait_nanoseconds));
    gl.glDeleteSync(written);
}

bool GlPackBuffer::packing() const
{
    AV_SCOPE("GlPackBuffer::packing");
    return buffer_ != 0u;
}

void GlPackBuffer::release(const glproto::RealGl &gl, GlHostImport &import)
{
    AV_SCOPE("GlPackBuffer::release");
    if (buffer_ != 0u && gl.glDeleteBuffers != nullptr) gl.glDeleteBuffers(1, &buffer_);
    if (memory_ != 0u) import.delete_memory(memory_);
    buffer_ = 0u;
    memory_ = 0u;
    stale_ = false;
}

/* A driver that will not make one is not asked again for this region. */
bool GlPackBuffer::make(const glproto::RealGl &gl, GlHostImport &import)
{
    AV_SCOPE("GlPackBuffer::make");
    refused_ = true;
    if (gl.glCreateBuffers == nullptr || gl.glNamedBufferStorageMemEXT == nullptr || gl.glBindBuffer == nullptr ||
        gl.glDeleteBuffers == nullptr ||
        gl.glFenceSync == nullptr || gl.glClientWaitSync == nullptr || gl.glDeleteSync == nullptr ||
        !import.imports_memory() || !import.import_memory(fd_, size_, memory_))
        return false;
    gl.glCreateBuffers(1, &buffer_);
    if (buffer_ != 0u) gl.glNamedBufferStorageMemEXT(buffer_, static_cast<GLsizeiptr>(size_), memory_, 0u);
    /* Storage the driver turned down leaves a buffer every transfer into it
     * fails on: it is not kept. */
    const bool stored = buffer_ != 0u && (gl.glGetError == nullptr || gl.glGetError() == 0u);
    if (!stored) {
        release(gl, import);
        return false;
    }
    refused_ = false;
    return true;
}

}  // namespace av
