#include "icd/vendor/adl_root.hpp"

#include "core/platform/thread_lock_hold.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "icd/vendor/ddi/adl_adapter_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_record.hpp"
#include "icd/vendor/ddi/adl_versions_info_x2_record.hpp"

namespace av::vendor {

AdlRoot::AdlRoot(Kmt &kmt, ThreadLock &lock) : lock_(lock), adapters_(kmt), opened_(0u)
{
    AV_SCOPE("AdlRoot::AdlRoot");
}

int AdlRoot::create(bool has_callback)
{
    AV_SCOPE("AdlRoot::create");
    if (!has_callback) return invalid_callback;
    const ThreadLockHold held(lock_);
    if (adapters_.learn() != Status::ok || adapters_.count_of(PciIdentity::amd_vendor) == 0u) return error;
    ++opened_;
    return ok;
}

int AdlRoot::destroy()
{
    AV_SCOPE("AdlRoot::destroy");
    const ThreadLockHold held(lock_);
    if (opened_ != 0u) --opened_;
    return ok;
}

int AdlRoot::refresh()
{
    AV_SCOPE("AdlRoot::refresh");
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    return adapters_.learn() == Status::ok ? ok : error;
}

void *AdlRoot::context()
{
    AV_SCOPE("AdlRoot::context");
    return this;
}

bool AdlRoot::holds(const void *context) const
{
    AV_SCOPE("AdlRoot::holds");
    return context == nullptr || context == this;
}

int AdlRoot::number_of_adapters(int *count)
{
    AV_SCOPE("AdlRoot::number_of_adapters");
    if (count == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    *count = static_cast<int>(adapters_.count_of(PciIdentity::amd_vendor));
    return ok;
}

int AdlRoot::adapter_info(AdlAdapterInfoRecord *infos, int bytes)
{
    AV_SCOPE("AdlRoot::adapter_info");
    if (infos == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    const uint32_t count = adapters_.count_of(PciIdentity::amd_vendor);
    if (bytes < 0 || static_cast<uint64_t>(bytes) < uint64_t{count} * sizeof(AdlAdapterInfoRecord))
        return invalid_parameter_size;
    for (uint32_t index = 0u; index < count; ++index) describe(index, infos[index]);
    return ok;
}

int AdlRoot::active(int index, int *status)
{
    AV_SCOPE("AdlRoot::active");
    if (status == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    if (adapter(index) == nullptr) return invalid_adapter_index;
    *status = 1;
    return ok;
}

int AdlRoot::asic_family_type(int index, int *types, int *valids)
{
    AV_SCOPE("AdlRoot::asic_family_type");
    if (types == nullptr || valids == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    const VendorAdapter *const asked = adapter(index);
    if (asked == nullptr) return invalid_adapter_index;
    *types = asked->facts().integrated() ? asic_integrated : asic_discrete;
    *valids = asic_discrete | asic_integrated;
    return ok;
}

int AdlRoot::primary(int *index)
{
    AV_SCOPE("AdlRoot::primary");
    if (index == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    const uint32_t count = adapters_.count_of(PciIdentity::amd_vendor);
    for (uint32_t at = 0u; at < count; ++at) {
        if (!adapters_.nth_of(PciIdentity::amd_vendor, at)->primary()) continue;
        *index = static_cast<int>(at);
        return ok;
    }
    return error;
}

int AdlRoot::versions(AdlVersionsInfoRecord *versions)
{
    AV_SCOPE("AdlRoot::versions");
    if (versions == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    *versions = AdlVersionsInfoRecord{};
    driver_version(versions->strDriverVer, sizeof versions->strDriverVer);
    return ok;
}

int AdlRoot::versions_x2(AdlVersionsInfoX2Record *versions)
{
    AV_SCOPE("AdlRoot::versions_x2");
    if (versions == nullptr) return null_pointer;
    const ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    *versions = AdlVersionsInfoX2Record{};
    driver_version(versions->strDriverVer, sizeof versions->strDriverVer);
    return ok;
}

const VendorAdapter *AdlRoot::adapter(int index) const
{
    AV_SCOPE("AdlRoot::adapter");
    if (index < 0) return nullptr;
    return adapters_.nth_of(PciIdentity::amd_vendor, static_cast<uint32_t>(index));
}

/* The hardware ID is the one Windows gave the device, which an application
 * matches against SetupAPI's; it names no instance, so the display path and
 * the driver's registry key are left empty rather than guessed. */
void AdlRoot::describe(uint32_t index, AdlAdapterInfoRecord &info) const
{
    AV_SCOPE("AdlRoot::describe");
    const VendorAdapter &adapter = *adapters_.nth_of(PciIdentity::amd_vendor, index);
    info = AdlAdapterInfoRecord{};
    info.iSize = static_cast<int>(sizeof info);
    info.iAdapterIndex = static_cast<int>(index);
    info.iBusNumber = static_cast<int>(adapter.facts().location().bus());
    info.iDeviceNumber = static_cast<int>(adapter.facts().location().device());
    info.iFunctionNumber = static_cast<int>(adapter.facts().location().function());
    info.iVendorID = static_cast<int>(adapter.facts().identity().vendor());
    info.iPresent = 1;
    info.iExist = 1;
    info.iOSDisplayIndex = -1;
    TextBuffer udid(info.strUDID, sizeof info.strUDID);
    udid.append("PCI_");
    adapter.facts().identity().append_hardware_id(udid);
    TextBuffer pnp(info.strPNPString, sizeof info.strPNPString);
    pnp.append("PCI\\");
    adapter.facts().identity().append_hardware_id(pnp);
    TextBuffer name(info.strAdapterName, sizeof info.strAdapterName);
    name.append(adapter.facts().name());
}

void AdlRoot::driver_version(char *out, uint32_t capacity) const
{
    AV_SCOPE("AdlRoot::driver_version");
    const VendorAdapter *const first = adapters_.nth_of(PciIdentity::amd_vendor, 0u);
    TextBuffer                 text(out, capacity);
    if (first != nullptr) first->facts().append_driver_version(text);
}

}  // namespace av::vendor
