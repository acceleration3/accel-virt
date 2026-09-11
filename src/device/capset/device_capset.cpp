#include "device/capset/device_capset.hpp"

#include "device/context/device_context_table.hpp"
#include "device/host/host.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

DeviceCapset::DeviceCapset(Host &host) : host_(host), info_(), described_(false) {}

/* Vulkan is the only sub-API offered: it is the first one executed. A device
 * type Vulkan does not define is reported as other, so the capset stays
 * decodable. */
void DeviceCapset::describe(uint32_t gpu_index)
{
    host_.require_main_loop("DeviceCapset::describe");
    const GpuIdentity gpu = host_.gpu_identity(gpu_index);
    const uint32_t type = gpu.device_type() > GpuIdentity::cpu ? GpuIdentity::other_gpu : gpu.device_type();
    const uint64_t features = host_.hostmem_size() != 0u ? CapsetInfo::feature_host_visible_memory : 0u;
    info_ = CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor),
                       static_cast<uint32_t>(SubApi::vulkan), DeviceContextTable::max_contexts,
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
