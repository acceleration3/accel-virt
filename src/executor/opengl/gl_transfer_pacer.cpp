#include "executor/opengl/gl_transfer_pacer.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/generated/opengl/gl_protocol_ids.h"

namespace av {

namespace {

constexpr uint32_t gl_sync_gpu_commands_complete = 0x9117u;
constexpr uint32_t gl_sync_flush_commands_bit = 0x00000001u;
constexpr uint64_t bytes_per_texel = 4u;
constexpr uint32_t most_values = 16u;

uint64_t magnitude(uint64_t wire)
{
    AV_SCOPE("magnitude");
    const int64_t value = static_cast<int32_t>(static_cast<uint32_t>(wire));
    return static_cast<uint64_t>(value < 0 ? -value : value);
}

uint64_t spread(uint64_t from, uint64_t to)
{
    AV_SCOPE("spread");
    const int64_t difference = static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(to))) -
                               static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(from)));
    return static_cast<uint64_t>(difference < 0 ? -difference : difference);
}


uint64_t buffer_bytes(const glproto::RealGl &gl, uint64_t which, bool named)
{
    AV_SCOPE("buffer_bytes");
    constexpr uint32_t gl_buffer_size = 0x8764u;
    int64_t size = 0;
    if (named && gl.glGetNamedBufferParameteri64v != nullptr)
        gl.glGetNamedBufferParameteri64v(static_cast<GLuint>(which), gl_buffer_size, &size);
    else if (!named && gl.glGetBufferParameteri64v != nullptr)
        gl.glGetBufferParameteri64v(static_cast<GLenum>(which), gl_buffer_size, &size);
    return size > 0 ? static_cast<uint64_t>(size) : 0u;
}

uint64_t framebuffer_bytes(const glproto::RealGl &gl)
{
    AV_SCOPE("framebuffer_bytes");
    constexpr uint32_t gl_viewport = 0x0BA2u;
    if (gl.glGetIntegerv == nullptr) return 0u;
    int32_t box[4] = {};
    gl.glGetIntegerv(gl_viewport, box);
    return magnitude(static_cast<uint64_t>(static_cast<uint32_t>(box[2]))) *
           magnitude(static_cast<uint64_t>(static_cast<uint32_t>(box[3]))) * bytes_per_texel;
}

}

GlTransferPacer::GlTransferPacer() : window_(), previous_(nullptr)
{
    AV_SCOPE("GlTransferPacer::GlTransferPacer");
}

uint64_t GlTransferPacer::cost(const glproto::RealGl &gl, uint32_t command_id, WireReader payload) const
{
    AV_SCOPE("GlTransferPacer::cost");
    uint32_t wanted = 0u;
    switch (command_id) {
    case glproto::kCmd_glCopyBufferSubData:
    case glproto::kCmd_glCopyNamedBufferSubData: wanted = 5u; break;
    case glproto::kCmd_glCopyImageSubData: wanted = 15u; break;
    case glproto::kCmd_glBlitFramebuffer: wanted = 8u; break;
    case glproto::kCmd_glBlitNamedFramebuffer: wanted = 10u; break;
    case glproto::kCmd_glClearTexSubImage: wanted = 8u; break;
    case glproto::kCmd_glClearBufferSubData:
    case glproto::kCmd_glClearNamedBufferSubData:
    case glproto::kCmd_glClearNamedBufferSubDataEXT: wanted = 4u; break;
    case glproto::kCmd_glClearBufferData:
    case glproto::kCmd_glClearNamedBufferData:
    case glproto::kCmd_glClearNamedBufferDataEXT: wanted = 1u; break;
    case glproto::kCmd_glClear:
    case glproto::kCmd_glClearBufferfv:
    case glproto::kCmd_glClearBufferiv:
    case glproto::kCmd_glClearBufferuiv:
    case glproto::kCmd_glClearBufferfi:
    case glproto::kCmd_glClearNamedFramebufferfv:
    case glproto::kCmd_glClearNamedFramebufferiv:
    case glproto::kCmd_glClearNamedFramebufferuiv:
    case glproto::kCmd_glClearNamedFramebufferfi: return framebuffer_bytes(gl);
    default: return 0u;
    }
    uint64_t values[most_values] = {};
    for (uint32_t at = 0u; at < wanted; at++)
        if (!payload.read_varint(values[at])) return 0u;
    switch (command_id) {
    case glproto::kCmd_glCopyBufferSubData:
    case glproto::kCmd_glCopyNamedBufferSubData: return values[4];
    case glproto::kCmd_glCopyImageSubData:
        return magnitude(values[12]) * magnitude(values[13]) * magnitude(values[14]) * bytes_per_texel;
    case glproto::kCmd_glClearBufferSubData:
    case glproto::kCmd_glClearNamedBufferSubData:
    case glproto::kCmd_glClearNamedBufferSubDataEXT: return values[3];
    case glproto::kCmd_glClearBufferData: return buffer_bytes(gl, values[0], false);
    case glproto::kCmd_glClearNamedBufferData:
    case glproto::kCmd_glClearNamedBufferDataEXT: return buffer_bytes(gl, values[0], true);
    case glproto::kCmd_glBlitFramebuffer:
        return spread(values[4], values[6]) * spread(values[5], values[7]) * bytes_per_texel;
    case glproto::kCmd_glBlitNamedFramebuffer:
        return spread(values[6], values[8]) * spread(values[7], values[9]) * bytes_per_texel;
    default: return magnitude(values[5]) * magnitude(values[6]) * magnitude(values[7]) * bytes_per_texel;
    }
}

void GlTransferPacer::ran(const glproto::RealGl &gl, uint32_t command_id, WireReader payload)
{
    AV_SCOPE("GlTransferPacer::ran");
    const uint64_t bytes = cost(gl, command_id, payload);
    if (bytes == 0u) return;
    window_.add(bytes);
    if (window_.full()) settle(gl);
}

void GlTransferPacer::settle(const glproto::RealGl &gl)
{
    AV_SCOPE("GlTransferPacer::settle");
    window_.forget();
    if (gl.glFenceSync == nullptr || gl.glClientWaitSync == nullptr || gl.glDeleteSync == nullptr) return;
    void *const marker = gl.glFenceSync(gl_sync_gpu_commands_complete, 0u);
    if (gl.glFlush != nullptr) gl.glFlush();
    if (previous_ != nullptr) {
        static_cast<void>(gl.glClientWaitSync(static_cast<GLsync>(previous_), gl_sync_flush_commands_bit,
                                              GpuWorkWindow::wait_nanoseconds));
        gl.glDeleteSync(static_cast<GLsync>(previous_));
    }
    previous_ = marker;
}

void GlTransferPacer::stop(const glproto::RealGl &gl)
{
    AV_SCOPE("GlTransferPacer::stop");
    window_.forget();
    if (previous_ != nullptr && gl.glDeleteSync != nullptr) gl.glDeleteSync(static_cast<GLsync>(previous_));
    previous_ = nullptr;
}

bool GlTransferPacer::invariants_hold() const
{
    AV_SCOPE("GlTransferPacer::invariants_hold");
    return window_.invariants_hold();
}

}
