/* One virtio-gpu control request the device core owns: its bytes, read through
 * the host from the guest's buffers, and its single response. Main loop. */
#ifndef AV_CONTROL_REQUEST_HPP
#define AV_CONTROL_REQUEST_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class GpuResponse;
class Host;

class ControlRequest {
public:
    ControlRequest(Host &host, void *request);
    ControlRequest(const ControlRequest &) = delete;
    ControlRequest &operator=(const ControlRequest &) = delete;

    /* Up to `size` bytes from `offset`: the count the request holds there. */
    size_t read(size_t offset, uint8_t *buffer, size_t size);
    /* Sends `response`; a second call sends nothing. */
    void   respond(const GpuResponse &response);
    bool   responded() const;

private:
    Host &host_;
    void *request_;
    bool  responded_;
};

}  // namespace av

#endif  // AV_CONTROL_REQUEST_HPP
