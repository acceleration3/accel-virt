/* The DRM format whose bytes in memory are a DXGI format's, which is how a
 * host image handed out as a dma-buf names its pixels: to QEMU's display
 * (HostImageExport) and to a process outside QEMU (ExportableImages). An sRGB
 * format and its linear sibling are one DRM format, since the encoding is the
 * reader's to choose. */
#ifndef AV_DRM_FOURCC_HPP
#define AV_DRM_FOURCC_HPP

#include <stdint.h>

namespace av {

class DrmFourcc {
public:
    explicit DrmFourcc(uint32_t dxgi_format);

    /* 0 for a format no DRM format lays out the same. */
    uint32_t value() const;

private:
    uint32_t value_;
};

}  // namespace av

#endif  // AV_DRM_FOURCC_HPP
