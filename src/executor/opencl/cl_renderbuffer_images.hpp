/* The cl_mems one context made over a renderbuffer through a texture of its
 * storage (GlRenderbufferTexture), and what each was made through.
 *
 * The host's OpenCL knows such a cl_mem only as a texture's, so this is what
 * answers the guest as the API would for the renderbuffer it named -
 * clGetGLObjectInfo's type and name, clGetGLTextureInfo's refusal - and what
 * says which texture and image go once the guest's last reference to the
 * cl_mem does. */
#ifndef AV_CL_RENDERBUFFER_IMAGES_HPP
#define AV_CL_RENDERBUFFER_IMAGES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct ClRenderbufferImagesProbe;

/* What one cl_mem was made over: its renderbuffer, and the texture and image
 * it was made through. */
struct RenderbufferImage {
    uint32_t renderbuffer_;
    uint32_t texture_;
    void    *image_;
};

class ClRenderbufferImages {
public:
    /* As many as the guest shares of a GL framebuffer's renderbuffers. */
    explicit ClRenderbufferImages(BlockSource &blocks);
    ClRenderbufferImages(const ClRenderbufferImages &) = delete;
    ClRenderbufferImages &operator=(const ClRenderbufferImages &) = delete;

    /* `mem`, made over `renderbuffer` through `texture` and `image`:
     * invalid_parameter for a null mem, a zero renderbuffer or texture,
     * conflict for a mem already held, no_memory when the table cannot grow. */
    [[nodiscard]] Status remember(const void *mem, uint32_t renderbuffer, uint32_t texture, void *image);
    /* The renderbuffer `mem` was made over, or 0 for a mem not held. */
    uint32_t             renderbuffer_of(const void *mem) const;
    /* Forgets `mem`, answering the texture and the image it was made through.
     * False, with both left as they were, for a mem not held. */
    [[nodiscard]] bool   forget(const void *mem, uint32_t &texture, void *&image);
    /* Forgets whichever is held first, answering the same: false once none is,
     * so a caller tearing everything down loops until it is. */
    [[nodiscard]] bool   forget_any(uint32_t &texture, void *&image);
    uint32_t             count() const;

    /* Every held mem names a renderbuffer and a texture. */
    bool invariants_hold() const;

private:
    friend struct ClRenderbufferImagesProbe;

    KeyedSlots<uint64_t, RenderbufferImage> images_;
};

}  // namespace av

#endif  // AV_CL_RENDERBUFFER_IMAGES_HPP
