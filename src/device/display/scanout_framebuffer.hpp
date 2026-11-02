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
    static constexpr uint32_t max_stride = 0x7FFFFFFFu;

    ScanoutFramebuffer();

    [[nodiscard]] Status adopt(const SetScanoutBlobRequest &request, uint64_t blob_size);
    void                 forget();

    bool           adopted() const;
    const GpuRect &rect() const;
    uint32_t       width() const;
    uint32_t       height() const;
    uint32_t       stride() const;
    uint32_t       offset() const;
    uint32_t       fourcc() const;
    uint64_t       end() const;
    uint64_t       blob_size() const;
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

}

#endif
