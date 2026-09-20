/* One virtio-gpu pixel format, and what a host display needs to know about it:
 * the DRM fourcc naming the same bytes, and how wide a pixel is.
 *
 * A virtio-gpu format's name is the order of its bytes in memory and a DRM
 * fourcc's is that order read as one little-endian word, so B8G8R8A8 - bytes
 * B, G, R, A - is DRM_FORMAT_ARGB8888. The table is here rather than in the
 * QEMU glue because choosing one is a decision (constitution II), and it is
 * not asked of QEMU because qemu_pixman_to_drm_format knows five formats and
 * answers 0 for half of these. */
#ifndef AV_SCANOUT_FORMAT_HPP
#define AV_SCANOUT_FORMAT_HPP

#include <stdint.h>

namespace av {

class ScanoutFormat {
public:
    /* VIRTIO_GPU_FORMAT_*, named by the bytes in memory as virtio-gpu names
     * them. */
    static constexpr uint32_t b8g8r8a8 = 1u;
    static constexpr uint32_t b8g8r8x8 = 2u;
    static constexpr uint32_t a8r8g8b8 = 3u;
    static constexpr uint32_t x8r8g8b8 = 4u;
    static constexpr uint32_t r8g8b8a8 = 67u;
    static constexpr uint32_t x8b8g8r8 = 68u;
    static constexpr uint32_t a8b8g8r8 = 121u;
    static constexpr uint32_t r8g8b8x8 = 134u;

    explicit ScanoutFormat(uint32_t format);

    /* Whether this device shows it at all. Everything below answers 0 when it
     * does not, so a size computed from a format nobody checked cannot come
     * out plausible. */
    bool     scannable() const;
    uint32_t fourcc() const;
    uint32_t pixel_bytes() const;
    uint32_t format() const;

    bool invariants_hold() const;

private:
    uint32_t format_;
};

}  // namespace av

#endif  // AV_SCANOUT_FORMAT_HPP
