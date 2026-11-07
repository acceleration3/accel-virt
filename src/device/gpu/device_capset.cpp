#include "device/gpu/device_capset.hpp"

#include "core/trace/function_scope.hpp"
#include "device/context/device_context_table.hpp"
#include "device/gpu/adapter_name.hpp"
#include "device/host/host.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/context/host_ram_policy.hpp"
#include "proto/context/mem_info_text.hpp"
#include "proto/context/vram_policy.hpp"
#include "proto/submission/ring_header.hpp"

namespace av {

DeviceCapset::DeviceCapset(Host &host) : host_(host), info_(), vram_reserve_mib_(0u), host_ram_reserve_mib_(0u), described_(false), display_resolved_(false)
{
    AV_SCOPE("DeviceCapset::DeviceCapset");
}

void DeviceCapset::describe(uint32_t gpu_index, uint32_t sub_apis, uint64_t features)
{
    AV_SCOPE("DeviceCapset::describe");
    host_.require_main_loop("DeviceCapset::describe");
    if (sub_apis == 0u) return;
    const GpuIdentity gpu = host_.gpu_identity(gpu_index);
    const uint64_t total = host_total();
    const uint32_t type = gpu.device_type() > GpuIdentity::cpu ? GpuIdentity::other_gpu : gpu.device_type();
    info_ = CapsetInfo(ProtocolVersion(ProtocolVersion::current_major, ProtocolVersion::current_minor),
                       sub_apis, DeviceContextTable::max_contexts,
                       RingHeader::lowest_order, RingHeader::highest_order, features,
                       GpuIdentity(gpu.vendor_id(), gpu.device_id(), type, gpu.uuid(), VramPolicy(vram_reserve_mib_).cap_for(gpu.local_memory()),
                                   HostRamPolicy(host_ram_reserve_mib_).budget_for(total, host_.guest_memory_bytes()), AdapterName(gpu.name()).text()));
    info_.reserve_vram(vram_reserve_mib_);
    info_.floor_host_ram(HostRamPolicy(host_ram_reserve_mib_).floor_for(total));
    described_ = true;
}

uint64_t DeviceCapset::host_total()
{
    AV_SCOPE("DeviceCapset::host_total");
    char   text[256] = {};
    size_t length = 0u;
    if (host_.read_file("/proc/meminfo", text, sizeof text, length) != Status::ok) return 0u;
    return MemInfoText(text, length).total_bytes();
}

void DeviceCapset::reserve_host_ram(uint32_t mib)
{
    AV_SCOPE("DeviceCapset::reserve_host_ram");
    host_ram_reserve_mib_ = mib;
}

void DeviceCapset::reserve_vram(uint32_t mib)
{
    AV_SCOPE("DeviceCapset::reserve_vram");
    vram_reserve_mib_ = mib;
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

}
