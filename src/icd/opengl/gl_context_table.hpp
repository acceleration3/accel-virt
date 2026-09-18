/* The process's OpenGL contexts by the DHGLRC opengl32 holds for each
 * (data-model.md): a handle is a slot and a generation, so a stale handle - of
 * a context deleted, whose slot another took - and a forged one resolve to
 * nothing. The contexts themselves live in an ObjectPool. */
#ifndef AV_GLICD_GL_CONTEXT_TABLE_HPP
#define AV_GLICD_GL_CONTEXT_TABLE_HPP

#include "core/support/object_pool.hpp"
#include "core/support/status.hpp"
#include "icd/opengl/guest_gl_context.hpp"

#include <stdint.h>

namespace av {
class BlockSource;
class StateWriter;
}  // namespace av

namespace av::glicd {

struct GlContextTableProbe;

class GlContextTable {
public:
    static constexpr uint32_t max_contexts = 256u;

    GlContextTable(BlockSource &memory, icd::IcdConnection &connection, icd::ThreadLock &send_lock, icd::LogSink &log,
                   const GlExtensionPolicy &extensions);
    GlContextTable(const GlContextTable &) = delete;
    GlContextTable &operator=(const GlContextTable &) = delete;

    /* A created context and its handle; no_space with max_contexts live,
     * no_memory when the pool cannot grow, and handle 0 either way. */
    [[nodiscard]] Status create(int32_t pixel_format, const GlContextOptions &options, uint64_t &handle);
    /* Null for a handle no live context has. */
    GuestGlContext      *resolve(uint64_t handle) const;
    uint64_t             handle_of(const GuestGlContext &context) const;
    /* invalid_parameter for a handle no live context has. */
    [[nodiscard]] Status destroy(uint64_t handle);
    uint32_t             live() const;
    void                 report(StateWriter &writer) const;

    bool invariants_hold() const;

private:
    static constexpr uint32_t generation_shift = 16u;
    static constexpr uint64_t slot_mask = 0xFFFFu;
    static constexpr uint32_t max_generation = 0xFFFFu;

    uint32_t slot_of(uint64_t handle) const;

    BlockSource                &memory_;
    icd::IcdConnection               &connection_;
    icd::ThreadLock            &send_lock_;
    icd::LogSink               &log_;
    const GlExtensionPolicy    &extensions_;
    ObjectPool<GuestGlContext>  pool_;
    GuestGlContext             *contexts_[max_contexts];
    uint32_t                    generations_[max_contexts];
    uint32_t                    live_;

    friend struct GlContextTableProbe;
};

}  // namespace av::glicd

#endif  // AV_GLICD_GL_CONTEXT_TABLE_HPP
