#include "icd/opengl/chained_frames.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/common/slot_format.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/wire/wire_writer.hpp"

#include "gl_dispatch_client.h"
#include "gl_protocol_ids.h"

namespace av::glicd {

ChainedFrames::ChainedFrames() : slots_(), textures_{}, opened_(false)
{
    AV_SCOPE("ChainedFrames::ChainedFrames");
}

bool ChainedFrames::open(ClientTransport &wire, icd::WindowChains &chains, void *window, uint64_t luid,
                         uint32_t width, uint32_t height, bool srgb)
{
    AV_SCOPE("ChainedFrames::open");
    delete_textures(wire);
    opened_ = false;
    if (!slots_.open(wire, chains, window, luid, width, height, icd::SlotLayout::rgba_bottom_up, slot_count, 0u))
        return false;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (slots_.memory(slot) <= UINT32_MAX) continue;
        close(&wire);
        return false;
    }
    glproto::av_glCreateTextures(wire, gl_texture_2d, static_cast<GLsizei>(slot_count), textures_);
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (textures_[slot] == 0u) {
            close(&wire);
            return false;
        }
        glproto::av_glTextureParameteri(wire, textures_[slot], gl_texture_tiling, static_cast<GLint>(gl_linear_tiling));
        glproto::av_glTextureStorageMem2DEXT(wire, textures_[slot], 1, icd::SlotFormat(icd::SlotLayout::rgba_bottom_up).gl_storage(srgb),
                                              static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                                              static_cast<GLuint>(slots_.memory(slot)), 0u);
    }
    opened_ = true;
    return true;
}

bool ChainedFrames::opened() const
{
    AV_SCOPE("ChainedFrames::opened");
    return opened_;
}

bool ChainedFrames::fits(void *window, uint32_t width, uint32_t height) const
{
    AV_SCOPE("ChainedFrames::fits");
    return opened_ && slots_.fits(window, width, height);
}

void ChainedFrames::draw(ClientTransport &wire, uint32_t source, uint32_t slot)
{
    AV_SCOPE("ChainedFrames::draw");
    if (!opened_ || slot >= slot_count) return;
    glproto::av_glCopyImageSubData(wire, source, gl_texture_2d, 0, 0, 0, 0, textures_[slot], gl_texture_2d, 0, 0, 0, 0,
                                    static_cast<GLsizei>(slots_.width()), static_cast<GLsizei>(slots_.height()), 1);
    finish(wire);
}

void ChainedFrames::finish(ClientTransport &wire)
{
    AV_SCOPE("ChainedFrames::finish");
    WireWriter finished = wire.request_writer();
    static_cast<void>(wire.send_async(glproto::kCmd_glFinish, finished));
}

void ChainedFrames::attach(ClientTransport &wire, uint32_t framebuffer, uint32_t slot)
{
    AV_SCOPE("ChainedFrames::attach");
    if (!opened_ || slot >= slot_count) return;
    glproto::av_glNamedFramebufferTexture(wire, framebuffer, gl_color_attachment0, textures_[slot], 0);
}

bool ChainedFrames::renders_into(ClientTransport &wire, uint32_t framebuffer)
{
    AV_SCOPE("ChainedFrames::renders_into");
    return glproto::av_glCheckNamedFramebufferStatus(wire, framebuffer, gl_draw_framebuffer) == gl_framebuffer_complete;
}

uint32_t ChainedFrames::texture(uint32_t slot) const
{
    AV_SCOPE("ChainedFrames::texture");
    return slot < slot_count ? textures_[slot] : 0u;
}

void ChainedFrames::close(ClientTransport *wire)
{
    AV_SCOPE("ChainedFrames::close");
    if (wire != nullptr) {
        delete_textures(*wire);
        slots_.release(*wire);
    }
    slots_.close();
    for (uint32_t &texture : textures_) texture = 0u;
    opened_ = false;
}

bool ChainedFrames::invariants_hold() const
{
    AV_SCOPE("ChainedFrames::invariants_hold");
    for (uint32_t slot = 0u; slot < slot_count; slot++)
        if (opened_ && (textures_[slot] == 0u || slots_.memory(slot) == 0u)) return false;
    return (!opened_ || slots_.opened()) && slots_.invariants_hold();
}

void ChainedFrames::delete_textures(ClientTransport &wire)
{
    AV_SCOPE("ChainedFrames::delete_textures");
    bool held = false;
    for (uint32_t slot = 0u; slot < slot_count; slot++) held = held || textures_[slot] != 0u;
    if (!held) return;
    glproto::av_glDeleteTextures(wire, static_cast<GLsizei>(slot_count), textures_);
    for (uint32_t &texture : textures_) texture = 0u;
}

}
