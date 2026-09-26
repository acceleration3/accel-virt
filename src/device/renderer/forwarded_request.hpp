/* A control request QEMU forwarded to the renderer: its bytes, which the
 * renderer's host serves to a ControlRequest as QEMU's host serves a virtqueue
 * element's. A view: the message the bytes arrived in outlives it. */
#ifndef AV_FORWARDED_REQUEST_HPP
#define AV_FORWARDED_REQUEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ForwardedRequest {
public:
    ForwardedRequest(const uint8_t *bytes, size_t length);
    ForwardedRequest(const ForwardedRequest &) = delete;
    ForwardedRequest &operator=(const ForwardedRequest &) = delete;

    /* Up to `size` bytes from `offset`: the count copied, 0 from the end on. */
    size_t read(size_t offset, uint8_t *buffer, size_t size) const;

private:
    const uint8_t *bytes_;
    size_t         length_;
};

}  // namespace av

#endif  // AV_FORWARDED_REQUEST_HPP
