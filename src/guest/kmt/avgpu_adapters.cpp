#include "guest/kmt/avgpu_adapters.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"

namespace av {

AvgpuAdapters::AvgpuAdapters(Kmt &kmt)
    : kmt_(kmt), adapters_(), luids_(), identities_(), none_(), count_(0u), primary_(false)
{
    AV_SCOPE("AvgpuAdapters::AvgpuAdapters");
}

AvgpuAdapters::~AvgpuAdapters()
{
    AV_SCOPE("AvgpuAdapters::~AvgpuAdapters");
    close();
}

Status AvgpuAdapters::learn()
{
    AV_SCOPE("AvgpuAdapters::learn");
    close();
    uint32_t     handles[max_adapters] = {};
    uint32_t     enumerated = 0u;
    const Status status = kmt_.enumerate_adapters(handles, max_adapters, enumerated);
    if (status != Status::ok) return status;
    const uint32_t held = enumerated < max_adapters ? enumerated : max_adapters;
    for (uint32_t index = 0u; index < held; ++index) keep_or_close(handles[index]);
    /* A machine with no primary display has no adapter it is on: 0 is no LUID. */
    uint64_t primary = 0u;
    if (kmt_.primary_display_luid(primary) != Status::ok) primary = 0u;
    put_primary_first(primary);
    return Status::ok;
}

void AvgpuAdapters::keep_or_close(uint32_t adapter)
{
    AV_SCOPE("AvgpuAdapters::keep_or_close");
    if (adapter == 0u) return;
    const PciIdentity avgpu(PciIdentity::virtio_vendor, virtio_gpu_device, PciIdentity::virtio_vendor, av_subsystem);
    PciIdentity       identity;
    if (kmt_.query_identity(adapter, identity) == Status::ok && identity.same_subsystem_as(avgpu)) {
        uint64_t luid = 0u;
        if (kmt_.adapter_luid(adapter, luid) != Status::ok) luid = 0u;
        adapters_[count_] = adapter;
        luids_[count_] = luid;
        identities_[count_] = identity;
        ++count_;
        return;
    }
    /* Another GPU: a failed close leaves nothing to undo. */
    static_cast<void>(kmt_.close_adapter(adapter));
}

void AvgpuAdapters::put_primary_first(uint64_t primary_luid)
{
    AV_SCOPE("AvgpuAdapters::put_primary_first");
    const uint32_t at = index_of(primary_luid);
    if (at == count_) return;
    const uint32_t    adapter = adapters_[at];
    const uint64_t    luid = luids_[at];
    const PciIdentity identity = identities_[at];
    for (uint32_t index = at; index > 0u; --index) {
        adapters_[index] = adapters_[index - 1u];
        luids_[index] = luids_[index - 1u];
        identities_[index] = identities_[index - 1u];
    }
    adapters_[0] = adapter;
    luids_[0] = luid;
    identities_[0] = identity;
    primary_ = true;
}

uint32_t AvgpuAdapters::count() const
{
    AV_SCOPE("AvgpuAdapters::count");
    return count_;
}

uint32_t AvgpuAdapters::adapter(uint32_t index) const
{
    AV_SCOPE("AvgpuAdapters::adapter");
    return index < count_ ? adapters_[index] : 0u;
}

uint64_t AvgpuAdapters::luid(uint32_t index) const
{
    AV_SCOPE("AvgpuAdapters::luid");
    return index < count_ ? luids_[index] : 0u;
}

const PciIdentity &AvgpuAdapters::identity(uint32_t index) const
{
    AV_SCOPE("AvgpuAdapters::identity");
    return index < count_ ? identities_[index] : none_;
}

bool AvgpuAdapters::primary(uint32_t index) const
{
    AV_SCOPE("AvgpuAdapters::primary");
    return primary_ && index == 0u && count_ != 0u;
}

uint32_t AvgpuAdapters::index_of(uint64_t luid) const
{
    AV_SCOPE("AvgpuAdapters::index_of");
    if (luid == 0u) return count_;
    for (uint32_t index = 0u; index < count_; ++index)
        if (luids_[index] == luid) return index;
    return count_;
}

uint32_t AvgpuAdapters::take(uint32_t index)
{
    AV_SCOPE("AvgpuAdapters::take");
    if (index >= count_) return 0u;
    const uint32_t adapter = adapters_[index];
    adapters_[index] = 0u;
    return adapter;
}

void AvgpuAdapters::close()
{
    AV_SCOPE("AvgpuAdapters::close");
    for (uint32_t index = 0u; index < count_; ++index) {
        /* A failed close leaves nothing to undo. */
        if (adapters_[index] != 0u) static_cast<void>(kmt_.close_adapter(adapters_[index]));
        adapters_[index] = 0u;
        luids_[index] = 0u;
        identities_[index] = PciIdentity();
    }
    count_ = 0u;
    primary_ = false;
}

bool AvgpuAdapters::invariants_hold() const
{
    AV_SCOPE("AvgpuAdapters::invariants_hold");
    if (count_ > max_adapters) return false;
    if (primary_ && (count_ == 0u || luids_[0] == 0u)) return false;
    for (uint32_t index = count_; index < max_adapters; ++index)
        if (adapters_[index] != 0u || luids_[index] != 0u) return false;
    return true;
}

}  // namespace av
