#include "icd/vendor/vendor_adapters.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
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
    uint32_t     handles[max_adapters] = {};
    uint32_t     enumerated = 0u;
    const Status status = kmt_.enumerate_adapters(handles, max_adapters, enumerated);
    if (status != Status::ok) return status;
    /* A machine with no primary display has no adapter it is on: 0 is no LUID. */
    uint64_t primary = 0u;
    if (kmt_.primary_display_luid(primary) != Status::ok) primary = 0u;
    const PciIdentity avgpu(AdapterSearch::virtio_vendor, AdapterSearch::virtio_gpu_device,
                            AdapterSearch::virtio_vendor, AdapterSearch::av_subsystem);
    const uint32_t    held = enumerated < max_adapters ? enumerated : max_adapters;
    for (uint32_t index = 0u; index < held; ++index) {
        const uint32_t adapter = handles[index];
        if (adapter == 0u) continue;
        PciIdentity identity;
        if (kmt_.query_identity(adapter, identity) == Status::ok && identity.same_subsystem_as(avgpu) &&
            describe(adapter, identity, primary, adapters_[count_]))
            ++count_;
        /* A failed close leaves nothing to undo. */
        static_cast<void>(kmt_.close_adapter(adapter));
    }
    return Status::ok;
}

bool VendorAdapters::describe(uint32_t adapter, const PciIdentity &identity, uint64_t primary_luid,
                              VendorAdapter &out)
{
    AV_SCOPE("VendorAdapters::describe");
    uint64_t    luid = 0u;
    PciLocation location;
    uint16_t    name[VendorAdapter::name_bytes] = {};
    uint64_t    version = 0u;
    uint64_t    video = 0u;
    uint64_t    system = 0u;
    uint64_t    shared = 0u;
    if (kmt_.adapter_luid(adapter, luid) != Status::ok) return false;
    if (kmt_.query_adapter_location(adapter, location) != Status::ok) return false;
    if (kmt_.query_adapter_name(adapter, name, VendorAdapter::name_bytes) != Status::ok) return false;
    if (kmt_.query_kmd_driver_version(adapter, version) != Status::ok) return false;
    if (kmt_.query_segment_sizes(adapter, video, system, shared) != Status::ok) return false;
    /* A miniport from before the bit says nothing of the GPU's kind, which
     * reads as discrete: what the adapter presented before it existed. */
    UmdAdapterInfo info;
    const bool     integrated = kmt_.query_adapter_features(adapter, info) == Status::ok &&
                            info.offers(UmdAdapterInfo::feature_integrated_gpu);
    out = VendorAdapter(luid, identity, location, version, video, system, shared, integrated,
                        primary_luid != 0u && luid == primary_luid);
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
