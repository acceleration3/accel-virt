#include "core/memory/host_visible_blob.hpp"

#include "core/trace/function_scope.hpp"
#include "core/virtio/virtio_gpu_device.hpp"
#include "proto/virtio_gpu/gpu_response_reader.hpp"
#include "proto/virtio_gpu/av_request_writer.hpp"

namespace av {

HostVisibleBlob::HostVisibleBlob(Platform &platform)
    : platform_(platform), resource_(0u), offset_(0u), mapped_(false), request_{}, answer_{}
    {
        AV_SCOPE("HostVisibleBlob::HostVisibleBlob");
    }

HostVisibleBlob::HostVisibleBlob(Platform &platform, uint32_t resource, uint64_t offset)
    : platform_(platform), resource_(resource), offset_(offset), mapped_(true), request_{}, answer_{}
{
    AV_SCOPE("HostVisibleBlob::HostVisibleBlob");
}

Status HostVisibleBlob::create(VirtioGpuDevice &device, uint32_t resource_id, uint64_t bytes)
{
    AV_SCOPE("HostVisibleBlob::create");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::create");
    if (resource_ != 0u) return Status::invalid_state;
    if (resource_id == 0u || bytes == 0u || (bytes & (page_bytes - 1u)) != 0u)
        return Status::invalid_parameter;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.host_blob(resource_id, bytes));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, writer.length(), map_info);
    if (status != Status::ok) return status;
    resource_ = resource_id;
    return Status::ok;
}

Status HostVisibleBlob::map(VirtioGpuDevice &device, uint64_t offset)
{
    AV_SCOPE("HostVisibleBlob::map");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::map");
    if (resource_ == 0u || mapped_) return Status::invalid_state;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.map_blob(resource_, offset));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, writer.length(), map_info);
    if (status != Status::ok) return status;
    /* The device says how the guest may cache what it just mapped; anything
     * else is an answer this driver does not know how to hand a process. */
    if (map_info != map_cache_cached) return Status::malformed;
    offset_ = offset;
    mapped_ = true;
    return Status::ok;
}

Status HostVisibleBlob::unmap(VirtioGpuDevice &device)
{
    AV_SCOPE("HostVisibleBlob::unmap");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::unmap");
    if (resource_ == 0u || !mapped_) return Status::invalid_state;
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.unmap_blob(resource_));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, writer.length(), map_info);
    if (status != Status::ok) return status;
    offset_ = 0u;
    mapped_ = false;
    return Status::ok;
}

Status HostVisibleBlob::destroy(VirtioGpuDevice &device)
{
    AV_SCOPE("HostVisibleBlob::destroy");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::destroy");
    if (resource_ == 0u) return Status::invalid_state;
    if (mapped_) {
        const Status unmapped = unmap(device);
        if (unmapped != Status::ok) return unmapped;
    }
    AvRequestWriter writer(request_, sizeof request_);
    static_cast<void>(writer.unref(resource_));
    uint32_t     map_info = 0u;
    const Status status = exchange(device, writer.length(), map_info);
    if (status != Status::ok) return status;
    resource_ = 0u;
    return Status::ok;
}

/* One request, one answer: an error the device answers with is the device
 * refusing, not a framing mistake of ours. */
Status HostVisibleBlob::exchange(VirtioGpuDevice &device, size_t length, uint32_t &map_info)
{
    AV_SCOPE("HostVisibleBlob::exchange");
    uint32_t     written = 0u;
    const Status answered = device.ask_control(request_, length, answer_, sizeof answer_, written);
    if (answered != Status::ok) return answered;
    GpuResponseReader reader;
    const Status      decoded = reader.decode(answer_, written);
    if (decoded != Status::ok) return decoded;
    if (!reader.ok()) return Status::not_supported;
    map_info = reader.map_info();
    return Status::ok;
}

void HostVisibleBlob::abandon()
{
    AV_SCOPE("HostVisibleBlob::abandon");
    platform_.require_irql_at_most(Irql::passive, "HostVisibleBlob::abandon");
    resource_ = 0u;
    offset_ = 0u;
    mapped_ = false;
}

bool     HostVisibleBlob::created() const
{
    AV_SCOPE("HostVisibleBlob::created");
    return resource_ != 0u;
}
bool     HostVisibleBlob::mapped() const
{
    AV_SCOPE("HostVisibleBlob::mapped");
    return mapped_;
}
uint32_t HostVisibleBlob::resource() const
{
    AV_SCOPE("HostVisibleBlob::resource");
    return resource_;
}
uint64_t HostVisibleBlob::offset() const
{
    AV_SCOPE("HostVisibleBlob::offset");
    return mapped_ ? offset_ : 0u;
}

bool HostVisibleBlob::invariants_hold() const
{
    AV_SCOPE("HostVisibleBlob::invariants_hold");
    return !mapped_ || resource_ != 0u;
}

}  // namespace av
