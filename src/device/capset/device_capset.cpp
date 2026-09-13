#include "device/capset/device_capset.hpp"

#include "device/context/device_context_table.hpp"
#include "device/host/host.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

DeviceCapset::DeviceCapset(Host &host) : host_(host), info_(), described_(false) {}

/* A device type Vulkan does not define is reported as other, so the capset
 * stays decodable. */
void DeviceCapset::describe(uint32_t gpu_index, uint32_t sub_apis, uint64_t features)
{
    host_.require_main_loop("DeviceCapset::describe");
    if (sub_apis == 0u) return;
    const GpuIdentity gpu = host_.gpu_identity(gpu_index);
    const uint32_t type = gpu.device_type() > GpuIdentity::cpu ? GpuIdentity::other_gpu : gpu.device_type();
    info_ = CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor),
                       sub_apis, DeviceContextTable::max_contexts,
                       RingHeader::lowest_order, RingHeader::highest_order, features,
                       GpuIdentity(gpu.vendor_id(), gpu.device_id(), type, gpu.uuid(), gpu.local_memory(),
                                   gpu.shared_memory(), gpu.name()));
    described_ = true;
}

bool DeviceCapset::described() const
{
    return described_;
}

const CapsetInfo &DeviceCapset::info() const
{
    return info_;
}

}  // namespace av
