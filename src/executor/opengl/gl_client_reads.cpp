#include "executor/opengl/gl_client_reads.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t pixel_pack_buffer = 0x88EBu;
constexpr uint32_t pixel_pack_binding = 0x88EDu;
constexpr uint32_t stream_read = 0x88E1u;
constexpr uint32_t gl_short = 0x1402u, gl_unsigned_short = 0x1403u, gl_int = 0x1404u, gl_unsigned_int = 0x1405u,
                   gl_float = 0x1406u, gl_half_float = 0x140Bu;
constexpr uint8_t  inline_client_pixels = 1u;
constexpr uint64_t most_bytes = 1u << 20u;

}

GlClientReads::GlClientReads() : scratch_(0u)
{
    AV_SCOPE("GlClientReads::GlClientReads");
}

bool GlClientReads::wants(uint32_t type) const
{
    AV_SCOPE("GlClientReads::wants");
    return type == gl_short || type == gl_unsigned_short || type == gl_int || type == gl_unsigned_int ||
           type == gl_float || type == gl_half_float;
}

bool GlClientReads::run(const glproto::RealGl &gl, WireReader payload, WireWriter &reply, WireArena &arena)
{
    AV_SCOPE("GlClientReads::run");
    uint64_t x = 0u, y = 0u, width = 0u, height = 0u, format = 0u, type = 0u, bytes = 0u;
    uint8_t  present = 0u;
    if (!payload.read_varint(x) || !payload.read_varint(y) || !payload.read_varint(width) ||
        !payload.read_varint(height) || !payload.read_varint(format) || !payload.read_varint(type))
        return false;
    if (!wants(static_cast<uint32_t>(type)) || !payload.read_pod(present) || present != inline_client_pixels ||
        !payload.read_varint(bytes) || bytes == 0u || bytes > most_bytes)
        return false;
    if (gl.glGenBuffers == nullptr || gl.glBindBuffer == nullptr || gl.glBufferData == nullptr ||
        gl.glGetBufferSubData == nullptr || gl.glReadPixels == nullptr || gl.glGetIntegerv == nullptr)
        return false;
    GLint bound = 0;
    gl.glGetIntegerv(pixel_pack_binding, &bound);
    if (bound != 0) return false;
    uint8_t *const zeros = arena.allocate<uint8_t>(static_cast<size_t>(bytes) + 1u);
    if (zeros == nullptr) return false;
    for (size_t at = 0u; at <= bytes; at++) zeros[at] = 0u;
    if (scratch_ == 0u) {
        GLuint made = 0u;
        gl.glGenBuffers(1, &made);
        scratch_ = made;
    }
    if (scratch_ == 0u) return false;
    gl.glBindBuffer(pixel_pack_buffer, scratch_);
    gl.glBufferData(pixel_pack_buffer, static_cast<GLsizeiptr>(bytes), zeros, stream_read);
    gl.glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y), static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height), static_cast<GLenum>(format), static_cast<GLenum>(type), nullptr);
    gl.glGetBufferSubData(pixel_pack_buffer, 0, static_cast<GLsizeiptr>(bytes), zeros);
    gl.glBindBuffer(pixel_pack_buffer, 0u);
    reply.write_varint(bytes);
    reply.write(zeros, static_cast<size_t>(bytes));
    return true;
}

void GlClientReads::release(const glproto::RealGl &gl)
{
    AV_SCOPE("GlClientReads::release");
    if (scratch_ != 0u && gl.glDeleteBuffers != nullptr) {
        const GLuint name = scratch_;
        gl.glDeleteBuffers(1, &name);
    }
    scratch_ = 0u;
}

bool GlClientReads::invariants_hold() const
{
    AV_SCOPE("GlClientReads::invariants_hold");
    return true;
}

}
