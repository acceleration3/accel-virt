#include "device/gpu/device_capset.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/device_context_table.hpp"
#include "device/gpu/adapter_name.hpp"
#include "device/host/host.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

DeviceCapset::DeviceCapset(Host &host) : host_(host), info_(), described_(false), display_resolved_(false)
{
    AV_SCOPE("DeviceCapset::DeviceCapset");
}

/* A device type Vulkan does not define is reported as other, so the capset
 * stays decodable. The name is the one the guest shows for the adapter, not
 * the driver's own (AdapterName). */
void DeviceCapset::describe(uint32_t gpu_index, uint32_t sub_apis, uint64_t features)
{
    AV_SCOPE("DeviceCapset::describe");
    host_.require_main_loop("DeviceCapset::describe");
    if (sub_apis == 0u) return;
    const GpuIdentity gpu = host_.gpu_identity(gpu_index);
    const uint32_t type = gpu.device_type() > GpuIdentity::cpu ? GpuIdentity::other_gpu : gpu.device_type();
    info_ = CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor),
                       sub_apis, DeviceContextTable::max_contexts,
                       RingHeader::lowest_order, RingHeader::highest_order, features,
                       GpuIdentity(gpu.vendor_id(), gpu.device_id(), type, gpu.uuid(), gpu.local_memory(),
                                   gpu.shared_memory(), AdapterName(gpu.name()).text()));
    described_ = true;
}

void DeviceCapset::withdraw_guest_import()
{
    AV_SCOPE("DeviceCapset::withdraw_guest_import");
    if (!described_) return;
    info_.withdraw(CapsetInfo::feature_guest_memory_import);
}

void DeviceCapset::resolve_display(bool takes_dmabuf)
{
    AV_SCOPE("DeviceCapset::resolve_display");
    host_.require_main_loop("DeviceCapset::resolve_display");
    if (display_resolved_) return;
    display_resolved_ = true;
    if (described_ && takes_dmabuf) info_.announce(CapsetInfo::feature_dmabuf_scanout);
    host_.log(takes_dmabuf ? "virtio-gpu-av: the display shows dma-bufs; blobs may be scanned out"
                           : "virtio-gpu-av: the display shows no dma-buf; the guest keeps the 2D path");
}

bool DeviceCapset::display_resolved() const
{
    AV_SCOPE("DeviceCapset::display_resolved");
    return display_resolved_;
}

bool DeviceCapset::described() const
{
    AV_SCOPE("DeviceCapset::described");
    return described_;
}

const CapsetInfo &DeviceCapset::info() const
{
    AV_SCOPE("DeviceCapset::info");
    return info_;
}

}  // namespace av
