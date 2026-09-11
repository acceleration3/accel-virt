#include "device/control/control_request.hpp"

#include "device/host/host.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

namespace av {

ControlRequest::ControlRequest(Host &host, void *request) : host_(host), request_(request), responded_(false) {}

size_t ControlRequest::read(size_t offset, uint8_t *buffer, size_t size)
{
    host_.require_main_loop("ControlRequest::read");
    return host_.read_request(request_, offset, buffer, size);
}

void ControlRequest::respond(const GpuResponse &response)
{
    host_.require_main_loop("ControlRequest::respond");
    if (responded_) return;
    host_.respond(request_, response.bytes(), response.length());
    responded_ = true;
}

bool ControlRequest::responded() const
{
    return responded_;
}

}  // namespace av
