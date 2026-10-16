/* The process's OpenGL contexts by the DHGLRC opengl32 holds for each
 * (data-model.md), as many as the application makes. A handle is minted once
 * and not again until the 32-bit count wraps - a DHGLRC is a pointer, 32 bits
 * on x86 - so a stale handle, of a context deleted, and a forged one resolve
 * to nothing. The contexts themselves live in an ObjectPool. */
#ifndef AV_GLICD_GL_CONTEXT_TABLE_HPP
#define AV_GLICD_GL_CONTEXT_TABLE_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/opengl/guest_gl_context.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class KeyedLog;
class StateWriter;
}  // namespace av

namespace av::icd {
class SharedHandles;
}

namespace av::glicd {

struct GlContextTableProbe;

class GlContextTable {
public:
    GlContextTable(BlockSource &memory, icd::IcdConnection &connection, ThreadLock &send_lock, LogSink &log,
                   KeyedLog &lines, const GlExtensionPolicy &extensions, icd::SharedHandles &handles);
    GlContextTable(const GlContextTable &) = delete;
    GlContextTable &operator=(const GlContextTable &) = delete;

    /* A created context and its handle; no_memory, and handle 0, when there
     * is no room for it. */
    [[nodiscard]] Status create(int32_t pixel_format, const GlContextOptions &options, uint64_t &handle);
    /* Null for a handle no live context has. */
    GuestGlContext      *resolve(uint64_t handle) const;
    /* invalid_parameter for a handle no live context has. */
    [[nodiscard]] Status destroy(uint64_t handle);
    uint32_t             live() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t last_handle = 0xFFFFFFFFu;

    /* The next handle no live context has. */
    uint64_t mint();

    BlockSource                &memory_;
    icd::IcdConnection               &connection_;
    ThreadLock                 &send_lock_;
    LogSink               &log_;
    KeyedLog              &lines_;
    const GlExtensionPolicy    &extensions_;
    icd::SharedHandles         &handles_;
    ObjectPool<GuestGlContext>  pool_;
    KeyedSlots<uint64_t, GuestGlContext *> contexts_;
    uint32_t                    next_;

    friend struct GlContextTableProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_CONTEXT_TABLE_HPP
