#include "icd/opengl/pixel_staging_buffer.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opengl/buffer_pieces.hpp"
#include "proto/client/client_transport.hpp"

#include "gl_dispatch_client.h"

namespace av::glicd {

PixelStagingBuffer::PixelStagingBuffer() : name_(0u), staged_(Staged::nothing)
{
    AV_SCOPE("PixelStagingBuffer::PixelStagingBuffer");
}

/* Bytes the command carries itself, or a lease lends whole, need no buffer.
 * A size past what intptr_t holds is a negative one cast: never staged. */
bool PixelStagingBuffer::stage_upload(ClientTransport &wire, const void *pixels, size_t bytes)
{
    AV_SCOPE("PixelStagingBuffer::stage_upload");
    BufferPieces pieces(wire);
    if (!pieces.cuts_write(static_cast<intptr_t>(bytes), pixels)) return false;
    if (!bind_storage(wire, gl_pixel_unpack_buffer, bytes, gl_stream_draw)) return false;
    pieces.write(gl_pixel_unpack_buffer, 0, bytes, static_cast<const uint8_t *>(pixels));
    staged_ = Staged::upload;
    return true;
}

void PixelStagingBuffer::finish_upload(ClientTransport &wire)
{
    AV_SCOPE("PixelStagingBuffer::finish_upload");
    if (staged_ != Staged::upload) return;
    glproto::av_glBindBuffer(wire, gl_pixel_unpack_buffer, 0u);
    staged_ = Staged::nothing;
}

bool PixelStagingBuffer::stage_download(ClientTransport &wire, const void *pixels, size_t bytes)
{
    AV_SCOPE("PixelStagingBuffer::stage_download");
    BufferPieces pieces(wire);
    if (!pieces.cuts_read(static_cast<intptr_t>(bytes), pixels) || pieces.lends_whole(bytes)) return false;
    if (!bind_storage(wire, gl_pixel_pack_buffer, bytes, gl_stream_read)) return false;
    staged_ = Staged::download;
    return true;
}

void PixelStagingBuffer::finish_download(ClientTransport &wire, void *pixels, size_t bytes)
{
    AV_SCOPE("PixelStagingBuffer::finish_download");
    if (staged_ != Staged::download) return;
    BufferPieces(wire).read(gl_pixel_pack_buffer, 0, bytes, static_cast<uint8_t *>(pixels));
    glproto::av_glBindBuffer(wire, gl_pixel_pack_buffer, 0u);
    staged_ = Staged::nothing;
}

bool PixelStagingBuffer::bind_storage(ClientTransport &wire, uint32_t target, size_t bytes, uint32_t usage)
{
    AV_SCOPE("PixelStagingBuffer::bind_storage");
    if (name_ == 0u) glproto::av_glGenBuffers(wire, 1, &name_);
    if (name_ == 0u) return false;
    glproto::av_glBindBuffer(wire, target, name_);
    glproto::av_glBufferData(wire, target, static_cast<intptr_t>(bytes), nullptr, usage);
    return true;
}

void PixelStagingBuffer::release(ClientTransport &wire)
{
    AV_SCOPE("PixelStagingBuffer::release");
    if (name_ != 0u) glproto::av_glDeleteBuffers(wire, 1, &name_);
    forget();
}

void PixelStagingBuffer::forget()
{
    AV_SCOPE("PixelStagingBuffer::forget");
    name_ = 0u;
    staged_ = Staged::nothing;
}

uint32_t PixelStagingBuffer::name() const
{
    AV_SCOPE("PixelStagingBuffer::name");
    return name_;
}

/* A transfer is only ever staged in a buffer the host named. */
bool PixelStagingBuffer::invariants_hold() const
{
    AV_SCOPE("PixelStagingBuffer::invariants_hold");
    return staged_ == Staged::nothing || name_ != 0u;
}

}  // namespace av::glicd
