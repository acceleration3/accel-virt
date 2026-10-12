#include "icd/vendor/vendor_adapters.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/kmt.hpp"
#include "proto/escape/umd_adapter_info.hpp"

namespace av::vendor {

VendorAdapters::VendorAdapters(Kmt &kmt) : kmt_(kmt), adapters_(), count_(0u)
{
    AV_SCOPE("VendorAdapters::VendorAdapters");
}

Status VendorAdapters::learn()
{
    AV_SCOPE("VendorAdapters::learn");
    count_ = 0u;
    AvgpuAdapters avgpu(kmt_);
    const Status  status = avgpu.learn();
    if (status != Status::ok) return status;
    for (uint32_t index = 0u; index < avgpu.count(); ++index)
        if (describe(avgpu, index, adapters_[count_])) ++count_;
    return Status::ok;
}

bool VendorAdapters::describe(const AvgpuAdapters &avgpu, uint32_t index, VendorAdapter &out)
{
    AV_SCOPE("VendorAdapters::describe");
    const uint32_t adapter = avgpu.adapter(index);
    const uint64_t luid = avgpu.luid(index);
    PciLocation    location;
    uint16_t    name[VendorAdapter::name_bytes] = {};
    uint64_t    version = 0u;
    uint64_t    video = 0u;
    uint64_t    system = 0u;
    uint64_t    shared = 0u;
    if (luid == 0u) return false;
    if (kmt_.query_adapter_location(adapter, location) != Status::ok) return false;
    if (kmt_.query_adapter_name(adapter, name, VendorAdapter::name_bytes) != Status::ok) return false;
    if (kmt_.query_kmd_driver_version(adapter, version) != Status::ok) return false;
    if (kmt_.query_segment_sizes(adapter, video, system, shared) != Status::ok) return false;
    /* A miniport from before the bit says nothing of the GPU's kind, which
     * reads as discrete: what the adapter presented before it existed. */
    UmdAdapterInfo info;
    const bool     integrated = kmt_.query_adapter_features(adapter, info) == Status::ok &&
                            info.offers(UmdAdapterInfo::feature_integrated_gpu);
    out = VendorAdapter(luid, avgpu.identity(index), location, version, video, system, shared, integrated,
                        avgpu.primary(index));
    out.take_name(name);
    return true;
}

uint32_t VendorAdapters::count_of(uint32_t vendor) const
{
    AV_SCOPE("VendorAdapters::count_of");
    uint32_t found = 0u;
    for (uint32_t index = 0u; index < count_; ++index)
        if (adapters_[index].identity().vendor() == vendor) ++found;
    return found;
}

const VendorAdapter *VendorAdapters::nth_of(uint32_t vendor, uint32_t index) const
{
    AV_SCOPE("VendorAdapters::nth_of");
    uint32_t seen = 0u;
    for (uint32_t at = 0u; at < count_; ++at) {
        if (adapters_[at].identity().vendor() != vendor) continue;
        if (seen++ == index) return &adapters_[at];
    }
    return nullptr;
}

}  // namespace av::vendor
