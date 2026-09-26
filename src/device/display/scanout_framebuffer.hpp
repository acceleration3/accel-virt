/* One framebuffer inside a blob, as SET_SCANOUT_BLOB describes it: a format,
 * a size in pixels, a stride, an offset into the blob, and the rectangle of it
 * a head shows.
 *
 * Every term of the fit is the guest's, so every sum is taken in 64 bits and
 * checked before it is compared against the one number the device owns, the
 * blob's size. A framebuffer that refused its request holds nothing: the
 * caller is about to hand a host GPU a descriptor and these numbers, and
 * reading plausible zeros is better than reading half a request. */
#ifndef AV_SCANOUT_FRAMEBUFFER_HPP
#define AV_SCANOUT_FRAMEBUFFER_HPP

#include "core/support/status.hpp"
#include "device/display/scanout_format.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"

#include <stdint.h>

namespace av {

class SetScanoutBlobRequest;
struct ScanoutFramebufferProbe;

class ScanoutFramebuffer {
public:
    /* QEMU refuses a stride it cannot hold in an int (hw/display/virtio-gpu.c),
     * and so does this before one ever reaches it. */
    static constexpr uint32_t max_stride = 0x7FFFFFFFu;

    ScanoutFramebuffer();

    /* The framebuffer `request` describes inside a blob of `blob_size` bytes.
     * invalid_parameter, holding nothing, for a format this device does not
     * scan out, a zero side, a stride shorter than a row or wider than
     * max_stride, a last row that runs past the blob, and a rectangle that is
     * not inside the framebuffer. */
    [[nodiscard]] Status adopt(const SetScanoutBlobRequest &request, uint64_t blob_size);
    void                 forget();

    bool           adopted() const;
    const GpuRect &rect() const;
    uint32_t       width() const;
    uint32_t       height() const;
    uint32_t       stride() const;
    uint32_t       offset() const;
    uint32_t       fourcc() const;
    /* One past the last byte the framebuffer reads out of the blob. */
    uint64_t       end() const;
    uint64_t       blob_size() const;
    /* Whether `other` reads the same pixels the same way. */
    bool           same_picture(const ScanoutFramebuffer &other) const;

    bool invariants_hold() const;

private:
    ScanoutFormat format_;
    GpuRect       rect_;
    uint32_t      width_;
    uint32_t      height_;
    uint32_t      stride_;
    uint32_t      offset_;
    uint64_t      blob_size_;
    bool          adopted_;

    friend struct ScanoutFramebufferProbe;
};

}  // namespace av

#endif  // AV_SCANOUT_FRAMEBUFFER_HPP
