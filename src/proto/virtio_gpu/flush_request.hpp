#ifndef AV_FLUSH_REQUEST_HPP
#define AV_FLUSH_REQUEST_HPP

#include "core/support/status.hpp"
#include "proto/virtio_gpu/gpu_rect.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WireReader;

class FlushRequest {
public:
    static constexpr size_t encoded_size = 48u;

    FlushRequest();

    [[nodiscard]] Status decode(WireReader &reader);

    const GpuRect &rect() const;
    uint32_t       resource_id() const;
    bool           decoded() const;

private:
    static constexpr size_t header_size = 24u;

    void forget();

    GpuRect  rect_;
    uint32_t resource_id_;
    bool     decoded_;
};

}

#endif
