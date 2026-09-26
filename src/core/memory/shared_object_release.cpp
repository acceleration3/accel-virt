#include "core/memory/shared_object_release.hpp"

#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/rvk_request_writer.hpp"

namespace av {

SharedObjectRelease::SharedObjectRelease(Platform &platform) : platform_(platform), request_{}, answer_{}
{
    AV_SCOPE("SharedObjectRelease::SharedObjectRelease");
}

Status SharedObjectRelease::send(VirtioGpuDevice &device, uint64_t object_id)
{
    AV_SCOPE("SharedObjectRelease::send");
    platform_.require_irql_at_most(Irql::passive, "SharedObjectRelease::send");
    RvkRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.release_shared(object_id));
    uint32_t     answered = 0u;
    const Status asked = device.ask_control(request_, writer.length(), answer_, sizeof answer_, answered);
    if (asked != Status::ok) return asked;
    GpuResponseReader reader;
    const Status      decoded = reader.decode(answer_, answered);
    if (decoded != Status::ok) return decoded;
    return reader.ok() ? Status::ok : Status::not_supported;
}

}  // namespace av
