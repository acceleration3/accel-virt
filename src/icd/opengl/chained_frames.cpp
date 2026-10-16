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

/* A window's chain is made again at its new size by the chains; only a chain
 * of another window is closed (ChainSlots). */
bool ChainedFrames::open(ClientTransport &wire, icd::WindowChains &chains, void *window, uint64_t luid,
                         uint32_t width, uint32_t height, bool srgb)
{
    AV_SCOPE("ChainedFrames::open");
    delete_textures(wire);
    opened_ = false;
    if (!slots_.open(wire, chains, window, luid, width, height, icd::SlotLayout::rgba_bottom_up, slot_count))
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
        /* A slot is a linear image (the host's ExportableImages), and GL
         * takes the tiling of memory it imports from the texture over it:
         * left at its optimal default, it draws tiled into memory Direct3D
         * reads row by row, and the window shows streaks. */
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

/* glFinish is synchronous on the wire because the application waits for it;
 * here nothing waits but the batch, which the host ends only after running it. */
void ChainedFrames::draw(ClientTransport &wire, uint32_t source, uint32_t slot)
{
    AV_SCOPE("ChainedFrames::draw");
    if (!opened_ || slot >= slot_count) return;
    glproto::av_glCopyImageSubData(wire, source, gl_texture_2d, 0, 0, 0, 0, textures_[slot], gl_texture_2d, 0, 0, 0, 0,
                                    static_cast<GLsizei>(slots_.width()), static_cast<GLsizei>(slots_.height()), 1);
    WireWriter finish = wire.request_writer();
    static_cast<void>(wire.send_async(glproto::kCmd_glFinish, finish));
}

/* The textures go before the memory under them is released. Without a wire
 * both went with the host's context. */
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

/* OpenGL deletes nothing for a zero name, so a partly made set goes as it is. */
void ChainedFrames::delete_textures(ClientTransport &wire)
{
    AV_SCOPE("ChainedFrames::delete_textures");
    bool held = false;
    for (uint32_t slot = 0u; slot < slot_count; slot++) held = held || textures_[slot] != 0u;
    if (!held) return;
    glproto::av_glDeleteTextures(wire, static_cast<GLsizei>(slot_count), textures_);
    for (uint32_t &texture : textures_) texture = 0u;
}

}  // namespace av::glicd
