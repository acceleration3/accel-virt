#ifndef AV_SCANOUT_FRAMEBUFFER_PROBE_HPP
#define AV_SCANOUT_FRAMEBUFFER_PROBE_HPP

#include "device/display/scanout_framebuffer.hpp"

#include <stdint.h>

namespace av {

struct ScanoutFramebufferProbe {
    explicit ScanoutFramebufferProbe(ScanoutFramebuffer &target);

    void set_adopted(bool adopted);
    void set_format(uint32_t format);
    void set_width(uint32_t width);
    void set_height(uint32_t height);
    void set_stride(uint32_t stride);
    void set_offset(uint32_t offset);
    void set_blob_size(uint64_t size);

    ScanoutFramebuffer &framebuffer;
};

}

#endif
