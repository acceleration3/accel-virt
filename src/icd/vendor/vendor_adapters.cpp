#include "icd/vendor/vendor_adapters.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/kmt/avgpu_adapters.hpp"
#include "icd/common/adapter_memory.hpp"

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
    constexpr uint32_t needed = icd::AdapterFacts::luid_fact | icd::AdapterFacts::location_fact |
                                icd::AdapterFacts::name_fact | icd::AdapterFacts::driver_version_fact |
                                icd::AdapterFacts::segments_fact;
    icd::AdapterFacts facts;
    facts.learn(kmt_, avgpu.adapter(index));
    if (!facts.knows(needed)) return false;
    out = VendorAdapter(facts, avgpu.primary(index));
    return true;
}

uint32_t VendorAdapters::count_of(uint32_t vendor) const
{
    AV_SCOPE("VendorAdapters::count_of");
    uint32_t found = 0u;
    for (uint32_t index = 0u; index < count_; ++index)
        if (adapters_[index].facts().identity().vendor() == vendor) ++found;
    return found;
}

const VendorAdapter *VendorAdapters::nth_of(uint32_t vendor, uint32_t index) const
{
    AV_SCOPE("VendorAdapters::nth_of");
    uint32_t seen = 0u;
    for (uint32_t at = 0u; at < count_; ++at) {
        if (adapters_[at].facts().identity().vendor() != vendor) continue;
        if (seen++ == index) return &adapters_[at];
    }
    return nullptr;
}

Status VendorAdapters::local_memory_left(const VendorAdapter &adapter, uint64_t &bytes)
{
    AV_SCOPE("VendorAdapters::local_memory_left");
    bytes = 0u;
    AdapterSearch search(kmt_);
    const Status  found = search.find_avgpu(adapter.facts().luid());
    if (found != Status::ok) return found;
    icd::AdapterMemory memory(kmt_, search.adapter());
    const Status       read = memory.read();
    if (read != Status::ok) return read;
    bytes = memory.available(MemorySegmentGroup::local);
    return Status::ok;
}

}
