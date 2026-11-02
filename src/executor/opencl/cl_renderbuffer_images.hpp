#ifndef AV_CL_RENDERBUFFER_IMAGES_HPP
#define AV_CL_RENDERBUFFER_IMAGES_HPP

#include "core/support/keyed_slots.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

struct ClRenderbufferImagesProbe;

struct RenderbufferImage {
    uint32_t renderbuffer_;
    uint32_t texture_;
    void    *image_;
};

class ClRenderbufferImages {
public:
    explicit ClRenderbufferImages(BlockSource &blocks);
    ClRenderbufferImages(const ClRenderbufferImages &) = delete;
    ClRenderbufferImages &operator=(const ClRenderbufferImages &) = delete;

    [[nodiscard]] Status remember(const void *mem, uint32_t renderbuffer, uint32_t texture, void *image);
    uint32_t             renderbuffer_of(const void *mem) const;
    [[nodiscard]] bool   forget(const void *mem, uint32_t &texture, void *&image);
    [[nodiscard]] bool   forget_any(uint32_t &texture, void *&image);
    uint32_t             count() const;

    bool invariants_hold() const;

private:
    friend struct ClRenderbufferImagesProbe;

    KeyedSlots<uint64_t, RenderbufferImage> images_;
};

}

#endif
