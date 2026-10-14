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

#include <stdint.h>

namespace av {

struct ClRenderbufferImagesProbe;

class ClRenderbufferImages {
public:
    /* What one context shares of a GL framebuffer's renderbuffers at once; a
     * guest wanting more is refused at the create, where it can be told. */
    static constexpr uint32_t max_images = 64u;

    ClRenderbufferImages();
    ClRenderbufferImages(const ClRenderbufferImages &) = delete;
    ClRenderbufferImages &operator=(const ClRenderbufferImages &) = delete;

    /* `mem`, made over `renderbuffer` through `texture` and `image`. False for
     * a null mem, a zero renderbuffer or texture, a mem already held, and when
     * full. */
    [[nodiscard]] bool remember(const void *mem, uint32_t renderbuffer, uint32_t texture, void *image);
    /* The renderbuffer `mem` was made over, or 0 for a mem not held. */
    uint32_t           renderbuffer_of(const void *mem) const;
    /* Forgets `mem`, answering the texture and the image it was made through.
     * False, with both left as they were, for a mem not held. */
    [[nodiscard]] bool forget(const void *mem, uint32_t &texture, void *&image);
    /* Forgets whichever is held first, answering the same: false once none is,
     * so a caller tearing everything down loops until it is. */
    [[nodiscard]] bool forget_any(uint32_t &texture, void *&image);
    uint32_t           count() const;

    bool invariants_hold() const;

private:
    friend struct ClRenderbufferImagesProbe;

    /* The slot holding `mem`, or count_ for none. */
    uint32_t slot_of(const void *mem) const;
    void     forget_slot(uint32_t slot, uint32_t &texture, void *&image);

    /* One slot is the same index in all four; the first count_ are held. */
    const void *mems_[max_images];
    uint32_t    renderbuffers_[max_images];
    uint32_t    textures_[max_images];
    void       *images_[max_images];
    uint32_t    count_;
};

}  // namespace av

#endif  // AV_CL_RENDERBUFFER_IMAGES_HPP
