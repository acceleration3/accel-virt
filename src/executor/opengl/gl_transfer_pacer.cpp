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

}

GlTransferPacer::GlTransferPacer() : window_(), previous_(nullptr)
{
    AV_SCOPE("GlTransferPacer::GlTransferPacer");
}

uint64_t GlTransferPacer::cost(uint32_t command_id, WireReader payload) const
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
    const uint64_t bytes = cost(command_id, payload);
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
