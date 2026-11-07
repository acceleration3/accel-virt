#ifndef AV_GL_TEXTURE_TARGETS_HPP
#define AV_GL_TEXTURE_TARGETS_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class BlockSource;

class GlTextureTargets {
public:
    explicit GlTextureTargets(BlockSource &blocks);
    GlTextureTargets(const GlTextureTargets &) = delete;
    GlTextureTargets &operator=(const GlTextureTargets &) = delete;

    [[nodiscard]] Status note(uint32_t texture, uint32_t target);
    uint32_t             target_of(uint32_t texture) const;
    void                 forget(uint32_t texture);

    uint32_t count() const;

    bool invariants_hold() const;

private:
    KeyedSlots<uint32_t, uint32_t> targets_;
};

}

#endif
