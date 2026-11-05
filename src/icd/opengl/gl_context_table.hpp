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
}

namespace av::icd {
class SharedHandles;
}

namespace av::glicd {

struct GlContextTableProbe;

class GlContextTable {
public:
    GlContextTable(BlockSource &memory, icd::IcdConnection &connection, ThreadLock &send_lock,
                   ThreadLock &slice_lock, KeyedLog &lines, const GlExtensionPolicy &extensions, icd::SharedHandles &handles);
    GlContextTable(const GlContextTable &) = delete;
    GlContextTable &operator=(const GlContextTable &) = delete;

    [[nodiscard]] Status create(int32_t pixel_format, const GlContextOptions &options, uint64_t &handle);
    GuestGlContext      *resolve(uint64_t handle) const;
    [[nodiscard]] Status destroy(uint64_t handle);
    uint32_t             live() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t last_handle = 0xFFFFFFFFu;

    uint64_t mint();

    BlockSource                &memory_;
    icd::IcdConnection               &connection_;
    ThreadLock                 &send_lock_;
    ThreadLock                 &slice_lock_;
    KeyedLog              &lines_;
    const GlExtensionPolicy    &extensions_;
    icd::SharedHandles         &handles_;
    ObjectPool<GuestGlContext>  pool_;
    KeyedSlots<uint64_t, GuestGlContext *> contexts_;
    uint32_t                    next_;

    friend struct GlContextTableProbe;
};

}

#endif
