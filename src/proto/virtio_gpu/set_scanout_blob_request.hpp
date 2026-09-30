/* A virtio-gpu SET_SCANOUT_BLOB request (virtio 1.2 5.7.6.10): which head,
 * which blob, the framebuffer inside it and the rectangle of that framebuffer
 * the head shows.
 *
 * It checks its own framing and the two rules that are the record's own - the
 * padding word is zero, and planes 1 to 3 carry no stride and no offset,
 * because the display shows one plane and a second would be dropped in
 * silence. Every other rule needs the blob's size, which the record does not
 * know; ScanoutFramebuffer applies them. */
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

    /* From the start of the request; malformed leaves every field zero. */
    [[nodiscard]] Status decode(WireReader &reader);

    const GpuRect &rect() const;
    uint32_t       scanout_id() const;
    uint32_t       resource_id() const;
    uint32_t       width() const;
    uint32_t       height() const;
    uint32_t       format() const;
    /* Plane 0's, the only plane this record accepts. */
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

}  // namespace av

#endif  // AV_SET_SCANOUT_BLOB_REQUEST_HPP
