#include "device/control/control_request.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

namespace av {

ControlRequest::ControlRequest(Host &host, void *request) : host_(host), request_(request), responded_(false)
{
    AV_SCOPE("ControlRequest::ControlRequest");
}

size_t ControlRequest::read(size_t offset, uint8_t *buffer, size_t size)
{
    AV_SCOPE("ControlRequest::read");
    host_.require_main_loop("ControlRequest::read");
    return host_.read_request(request_, offset, buffer, size);
}

void ControlRequest::respond(const GpuResponse &response)
{
    AV_SCOPE("ControlRequest::respond");
    host_.require_main_loop("ControlRequest::respond");
    if (responded_) return;
    host_.respond(request_, response.bytes(), response.length());
    responded_ = true;
}

bool ControlRequest::responded() const
{
    AV_SCOPE("ControlRequest::responded");
    return responded_;
}

}  // namespace av
