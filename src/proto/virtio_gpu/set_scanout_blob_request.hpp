#ifndef AV_SET_SCANOUT_BLOB_REQUEST_HPP
#define AV_SET_SCANOUT_BLOB_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class SetScanoutBlobRequest {
public:
    static constexpr size_t   encoded_size = 96u;
    static constexpr uint32_t planes = 4u;

    SetScanoutBlobRequest();

    [[nodiscard]] Status decode(WireReader &reader);

    const GpuRect &rect() const;
    uint32_t       scanout_id() const;
    uint32_t       resource_id() const;
    uint32_t       width() const;
    uint32_t       height() const;
    uint32_t       format() const;
    uint32_t       stride() const;
    uint32_t       offset() const;
    bool           decoded() const;

private:
    static constexpr size_t header_size = 24u;

    void forget();

    GpuRect  rect_;
    uint32_t scanout_id_;
    uint32_t resource_id_;
    uint32_t width_;
    uint32_t height_;
    uint32_t format_;
    uint32_t stride_;
    uint32_t offset_;
    bool     decoded_;
};

}

#endif
