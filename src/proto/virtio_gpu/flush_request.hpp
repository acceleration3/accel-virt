/* A virtio-gpu RESOURCE_FLUSH request (virtio 1.2 5.7.6.6): the resource whose
 * pixels changed and the rectangle of it that did. The device core reads it
 * only for a request naming a blob; a 2D resource's flush is the base class's.
 *
 * It checks its own framing and its padding word; whether the rectangle fits
 * is the head's business, because only the head knows what it is showing. */
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

    /* From the start of the request; malformed leaves every field zero. */
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

}  // namespace av

#endif  // AV_FLUSH_REQUEST_HPP
