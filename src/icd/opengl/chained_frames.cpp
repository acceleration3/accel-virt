#include "icd/opengl/chained_frames.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/transport/shared_object_call.hpp"
#include "proto/client/client_transport.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/wire/wire_writer.hpp"

#include "gl_dispatch_client.h"
#include "gl_protocol_ids.h"

namespace av::glicd {

ChainedFrames::ChainedFrames()
    : window_(nullptr), width_(0u), height_(0u), ids_{}, bytes_{}, memories_{}, textures_{}, opened_(false)
{
    AV_SCOPE("ChainedFrames::ChainedFrames");
}

bool ChainedFrames::open(ClientTransport &wire, icd::WindowChains &chains, void *window, uint64_t luid,
                         uint32_t width, uint32_t height, bool srgb)
{
    AV_SCOPE("ChainedFrames::open");
    /* A window's chain is made again at its new size by the chains; only a
     * chain of another window is closed. */
    delete_slots(wire);
    void *const before = window_;
    forget();
    if (before != nullptr && before != window) chains.close(before);
    if (window == nullptr || width == 0u || height == 0u) return false;
    if (!chains.open(window, luid, width, height, icd::SlotLayout::rgba_bottom_up, slot_count, ids_, bytes_))
        return false;
    window_ = window;
    width_ = width;
    height_ = height;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        SharedObjectCall call(wire);
        uint64_t         memory = 0u;
        if (call.open(ids_[slot], SharedObjectKind::memory, bytes_[slot], memory) != Status::ok ||
            memory == 0u || memory > UINT32_MAX) {
            close(&wire, chains);
            return false;
        }
        memories_[slot] = static_cast<uint32_t>(memory);
    }
    glproto::av_glCreateTextures(wire, gl_texture_2d, static_cast<GLsizei>(slot_count), textures_);
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (textures_[slot] == 0u) {
            close(&wire, chains);
            return false;
        }
        glproto::av_glTextureStorageMem2DEXT(wire, textures_[slot], 1, srgb ? gl_srgb8_alpha8 : gl_rgba8,
                                              static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                                              memories_[slot], 0u);
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
    return opened_ && window == window_ && width == width_ && height == height_;
}

/* glFinish is synchronous on the wire because the application waits for it;
 * here nothing waits but the batch, which the host ends only after running it. */
void ChainedFrames::draw(ClientTransport &wire, uint32_t source, uint32_t slot)
{
    AV_SCOPE("ChainedFrames::draw");
    if (!opened_ || slot >= slot_count) return;
    glproto::av_glCopyImageSubData(wire, source, gl_texture_2d, 0, 0, 0, 0, textures_[slot], gl_texture_2d, 0, 0, 0, 0,
                                    static_cast<GLsizei>(width_), static_cast<GLsizei>(height_), 1);
    WireWriter finish = wire.request_writer();
    static_cast<void>(wire.send_async(glproto::kCmd_glFinish, finish));
}

void ChainedFrames::close(ClientTransport *wire, icd::WindowChains &chains)
{
    AV_SCOPE("ChainedFrames::close");
    if (wire != nullptr) delete_slots(*wire);
    if (window_ != nullptr) chains.close(window_);
    forget();
}

bool ChainedFrames::invariants_hold() const
{
    AV_SCOPE("ChainedFrames::invariants_hold");
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        const bool whole = memories_[slot] != 0u && textures_[slot] != 0u;
        if (opened_ && !whole) return false;
    }
    return !opened_ || (window_ != nullptr && width_ != 0u && height_ != 0u);
}

/* OpenGL deletes nothing for a zero name, so a partly opened set goes as it
 * is. A memory object is the device's to delete: releasing it does
 * (OpenGlBackend::release_object). */
void ChainedFrames::delete_slots(ClientTransport &wire)
{
    AV_SCOPE("ChainedFrames::delete_slots");
    bool held = false;
    for (uint32_t slot = 0u; slot < slot_count; slot++)
        held = held || memories_[slot] != 0u || textures_[slot] != 0u;
    if (!held) return;
    glproto::av_glDeleteTextures(wire, static_cast<GLsizei>(slot_count), textures_);
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        if (memories_[slot] == 0u) continue;
        SharedObjectCall call(wire);
        static_cast<void>(call.release(ids_[slot], SharedObjectKind::memory, bytes_[slot]));
    }
}

void ChainedFrames::forget()
{
    AV_SCOPE("ChainedFrames::forget");
    window_ = nullptr;
    width_ = 0u;
    height_ = 0u;
    for (uint32_t slot = 0u; slot < slot_count; slot++) {
        ids_[slot] = 0u;
        bytes_[slot] = 0u;
        memories_[slot] = 0u;
        textures_[slot] = 0u;
    }
    opened_ = false;
}

}  // namespace av::glicd
