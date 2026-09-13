/* One guest context's OpenGL, run on the host's own.
 *
 * Not the same shape as the other two. The generator gives Vulkan and OpenCL an
 * abstract dispatch to derive from; OpenGL gets a table of 1375 function
 * pointers and a decoder that reads it, so this owns the table, the context it
 * was resolved against, the fences a guest's tokens name and the region its
 * bulk-data commands read from. */
#ifndef AV_OPENGL_BACKEND_HPP
#define AV_OPENGL_BACKEND_HPP

#include "device/executor/guest_region.hpp"
#include "device/executor/session_backend.hpp"
#include "device/executor/sync_tokens.hpp"
#include "device/shim/gl_context.hpp"
#include "proto/generated/opengl/gl_real_dispatch.h"

#include <stdint.h>

namespace av {

class OpenGlBackend final : public SessionBackend {
public:
    OpenGlBackend();

    /* Opens a context on the GPU `uuid` names and fills the table from it.
     * False for a host that does not serve OpenGL on that GPU, which the
     * capset already said and this confirms. */
    [[nodiscard]] bool initialize(const uint8_t *uuid);
    void               shut_down();

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* How many of the table's entry points this host actually has. A driver
     * short of some is not a failure: the decoder refuses each command whose
     * entry point is null, one command at a time. */
    /* What the generated table's hooks call, which is why they are fields on
     * it: a parameterless hook could not reach one context's state. */
    [[nodiscard]] bool bind_fence(uint64_t token, void *fence);
    void              *fence_for(uint64_t token) const;
    void               forget_fence(uint64_t token);
    uint8_t           *guest_bytes(uint64_t offset, uint64_t bytes) const;
    /* The mapping this context's bulk-data commands read from. */
    void               take_region(uint8_t *base, uint64_t length);

    uint32_t resolved() const;
    uint64_t ran() const;

private:
    /* Every OpenGL command id carries this in its top 16 bits - 3, not the
     * capset's bit 2, which is a remap worth reading twice. */
    static constexpr uint32_t opengl_api = 3u;

    GlContext   context_;
    glproto::RealGl gl_;
    SyncTokens  fences_;
    GuestRegion region_;
    uint32_t    resolved_;
    uint64_t    ran_;
    bool        lost_;
};

}  // namespace av

#endif  // AV_OPENGL_BACKEND_HPP
