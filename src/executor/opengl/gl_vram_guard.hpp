#ifndef AV_GL_VRAM_GUARD_HPP
#define AV_GL_VRAM_GUARD_HPP

#include "core/support/keyed_slots.hpp"
#include "executor/vram_account.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class BlockSource;
class GlErrorQueue;
class ByteLedger;

class GlVramGuard {
public:
    GlVramGuard(BlockSource &blocks, glproto::RealGl &gl, GlErrorQueue &errors);
    GlVramGuard(const GlVramGuard &) = delete;
    GlVramGuard &operator=(const GlVramGuard &) = delete;

    void bind(ByteLedger *vram, ByteLedger *host);
    void install();
    void clear();

    bool admit(uint32_t command, const uint64_t *args);

    uint64_t held() const;
    uint64_t host_held() const;
    uint32_t objects() const;

    bool invariants_hold() const;

private:
    enum class Kind : uint64_t { buffer = 1u, texture = 2u, renderbuffer = 3u, host_buffer = 4u };

    [[nodiscard]] bool account(VramAccount &against, Kind kind, uint32_t name, uint32_t slot, uint64_t bytes);
    [[nodiscard]] bool buffer_bytes(uint32_t name, uint64_t size, bool resident);
    uint64_t           row_bytes(Kind kind, uint32_t name, uint32_t slot);
    [[nodiscard]] bool refuse();
    uint32_t           bound(uint32_t binding);
    bool               buffer_target(uint32_t target, uint64_t size, bool resident);
    bool               buffer_named(uint32_t name, uint64_t size, bool resident);
    bool               image(uint32_t target, uint32_t level, uint32_t format, uint64_t width, uint64_t height,
                             uint64_t depth, uint64_t samples);
    bool               storage(uint32_t texture, uint32_t target, uint64_t levels, uint32_t format, uint64_t width,
                               uint64_t height, uint64_t depth, uint64_t samples);
    bool               compressed(uint32_t target, uint32_t level, int64_t bytes);
    bool               renderbuffer(uint32_t name, uint32_t format, uint64_t width, uint64_t height,
                                    uint64_t samples);
    void               forget(Kind kind, uint64_t count, const uint32_t *names);

    glproto::RealGl &gl_;
    GlErrorQueue    &errors_;
    VramAccount      account_;
    VramAccount      host_;
    KeyedSlots<uint64_t, uint64_t> sizes_;
};

}

#endif
