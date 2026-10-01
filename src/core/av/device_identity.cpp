#include "core/av/device_identity.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/context/gpu_identity.hpp"

namespace av {

DeviceIdentity::DeviceIdentity(Platform &platform)
    : platform_(platform), started_on_(), known_(false), changed_(false)
    {
        AV_SCOPE("DeviceIdentity::DeviceIdentity");
    }

void DeviceIdentity::remember(const CapsetInfo &capset)
{
    AV_SCOPE("DeviceIdentity::remember");
    platform_.require_irql_at_most(Irql::passive, "DeviceIdentity::remember");
    started_on_ = capset;
    known_ = true;
    changed_ = false;
}

bool DeviceIdentity::accept(const CapsetInfo &capset)
{
    AV_SCOPE("DeviceIdentity::accept");
    platform_.require_irql_at_most(Irql::passive, "DeviceIdentity::accept");
    if (!known_) return true;
    if (same_gpu(capset) && same_protocol(capset)) return true;
    if (!changed_) platform_.log("avgpu: the device under this adapter is not the one it started on");
    changed_ = true;
    return false;
}

bool DeviceIdentity::known() const
{
    AV_SCOPE("DeviceIdentity::known");
    platform_.require_irql_at_most(Irql::dispatch, "DeviceIdentity::known");
    return known_;
}

bool DeviceIdentity::offers(uint64_t feature) const
{
    AV_SCOPE("DeviceIdentity::offers");
    return known_ && started_on_.has_feature(feature);
}

bool DeviceIdentity::changed() const
{
    AV_SCOPE("DeviceIdentity::changed");
    platform_.require_irql_at_most(Irql::dispatch, "DeviceIdentity::changed");
    return changed_;
}

void DeviceIdentity::forget()
{
    AV_SCOPE("DeviceIdentity::forget");
    platform_.require_irql_at_most(Irql::passive, "DeviceIdentity::forget");
    known_ = false;
    changed_ = false;
}

bool DeviceIdentity::same_gpu(const CapsetInfo &capset) const
{
    AV_SCOPE("DeviceIdentity::same_gpu");
    return started_on_.gpu().same_gpu(capset.gpu());
}

/* The same major, and no older a minor: the device agreed to speak what this
 * adapter started on, and cannot be asked to finish work in a dialect it has
 * since lost. */
bool DeviceIdentity::same_protocol(const CapsetInfo &capset) const
{
    AV_SCOPE("DeviceIdentity::same_protocol");
    return capset.version().major_version() == started_on_.version().major_version() &&
           capset.version().minor_version() >= started_on_.version().minor_version();
}

bool DeviceIdentity::invariants_hold() const
{
    AV_SCOPE("DeviceIdentity::invariants_hold");
    return known_ || !changed_;
}

}  // namespace av
