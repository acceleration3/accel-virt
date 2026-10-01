#include "device/control/control_request.hpp"

#include "core/trace/function_scope.hpp"
#include "device/host/host.hpp"
#include "proto/virtio_gpu/gpu_response.hpp"

#include <stdio.h>

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
    constexpr uint32_t first_error = 0x1200u;   /* VIRTIO_GPU_RESP_ERR_UNSPEC */
    if (response.type() >= first_error) {
        uint8_t bytes[sizeof(uint32_t)] = {};
        static_cast<void>(host_.read_request(request_, 0u, bytes, sizeof bytes));
        const uint32_t command = static_cast<uint32_t>(bytes[0]) | static_cast<uint32_t>(bytes[1]) << 8u;
        fprintf(stderr, "virtio-gpu-av: command 0x%04x answered 0x%04x\n", command, response.type());
    }
    host_.respond(request_, response.bytes(), response.length());
    responded_ = true;
}

bool ControlRequest::responded() const
{
    AV_SCOPE("ControlRequest::responded");
    return responded_;
}

}  // namespace av
