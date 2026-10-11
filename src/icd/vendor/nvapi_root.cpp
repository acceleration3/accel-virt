#include "icd/vendor/nvapi_root.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock_hold.hpp"
#include "icd/vendor/ddi/nv_display_driver_memory_info_record.hpp"

namespace av::vendor {

namespace {

/* MAKE_NVAPI_VERSION(NV_DISPLAY_DRIVER_MEMORY_INFO_Vn, n): the record's size
 * at version n, over n. */
constexpr uint32_t memory_info_v1 = 20u | (1u << 16u);
constexpr uint32_t memory_info_v2 = 24u | (2u << 16u);
constexpr uint32_t memory_info_v3 = 32u | (3u << 16u);
constexpr uint64_t kib = 1024u;

}  // namespace

NvapiRoot::NvapiRoot(Kmt &kmt, icd::ThreadLock &lock) : lock_(lock), adapters_(kmt), opened_(0u), displays_{}
{
    AV_SCOPE("NvapiRoot::NvapiRoot");
}

int NvapiRoot::initialize()
{
    AV_SCOPE("NvapiRoot::initialize");
    const icd::ThreadLockHold held(lock_);
    if (adapters_.learn() != Status::ok) return error;
    if (adapters_.count_of(nvidia_vendor) == 0u) return device_not_found;
    ++opened_;
    return ok;
}

int NvapiRoot::unload()
{
    AV_SCOPE("NvapiRoot::unload");
    const icd::ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    --opened_;
    return ok;
}

int NvapiRoot::enum_physical_gpus(void **gpus, uint32_t *count)
{
    AV_SCOPE("NvapiRoot::enum_physical_gpus");
    if (gpus == nullptr || count == nullptr) return invalid_argument;
    const icd::ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    /* Every adapter fits: VendorAdapters holds fewer than NVAPI's array. */
    static_assert(VendorAdapters::max_adapters <= max_physical_gpus);
    const uint32_t found = adapters_.count_of(nvidia_vendor);
    for (uint32_t index = 0u; index < found; ++index)
        gpus[index] = const_cast<VendorAdapter *>(adapters_.nth_of(nvidia_vendor, index));
    *count = found;
    return ok;
}

int NvapiRoot::full_name(const void *gpu, char *name)
{
    AV_SCOPE("NvapiRoot::full_name");
    if (name == nullptr) return invalid_argument;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    TextBuffer text(name, short_string);
    text.append(asked->name());
    return ok;
}

int NvapiRoot::pci_identifiers(const void *gpu, uint32_t *device_id, uint32_t *subsystem_id, uint32_t *revision,
                               uint32_t *external_device_id)
{
    AV_SCOPE("NvapiRoot::pci_identifiers");
    if (device_id == nullptr || subsystem_id == nullptr || revision == nullptr || external_device_id == nullptr)
        return invalid_argument;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    const PciIdentity &identity = asked->identity();
    *device_id = (identity.device() << 16u) | identity.vendor();
    *subsystem_id = (identity.subsystem() << 16u) | identity.subsystem_vendor();
    *revision = identity.revision();
    *external_device_id = *device_id;
    return ok;
}

int NvapiRoot::bus_id(const void *gpu, uint32_t *bus)
{
    AV_SCOPE("NvapiRoot::bus_id");
    if (bus == nullptr) return invalid_argument;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    *bus = asked->location().bus();
    return ok;
}

int NvapiRoot::bus_slot_id(const void *gpu, uint32_t *slot)
{
    AV_SCOPE("NvapiRoot::bus_slot_id");
    if (slot == nullptr) return invalid_argument;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    *slot = asked->location().device();
    return ok;
}

int NvapiRoot::gpu_type(const void *gpu, int *type)
{
    AV_SCOPE("NvapiRoot::gpu_type");
    if (type == nullptr) return invalid_argument;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    *type = asked->integrated() ? gpu_integrated : gpu_discrete;
    return ok;
}

int NvapiRoot::memory_info(const void *gpu, NvDisplayDriverMemoryInfoRecord *memory)
{
    AV_SCOPE("NvapiRoot::memory_info");
    if (memory == nullptr) return invalid_argument;
    const uint32_t version = memory->version;
    if (version != memory_info_v1 && version != memory_info_v2 && version != memory_info_v3)
        return incompatible_struct_version;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    const auto dedicated = static_cast<uint32_t>(asked->dedicated_video_bytes() / kib);
    memory->dedicatedVideoMemory = dedicated;
    memory->availableDedicatedVideoMemory = dedicated;
    memory->systemVideoMemory = static_cast<uint32_t>(asked->dedicated_system_bytes() / kib);
    memory->sharedSystemMemory = static_cast<uint32_t>(asked->shared_system_bytes() / kib);
    if (version == memory_info_v1) return ok;
    memory->curAvailableDedicatedVideoMemory = dedicated;
    if (version == memory_info_v2) return ok;
    memory->dedicatedVideoMemoryEvictionsSize = 0u;
    memory->dedicatedVideoMemoryEvictionCount = 0u;
    return ok;
}

int NvapiRoot::adapter_id(const void *gpu, void *luid)
{
    AV_SCOPE("NvapiRoot::adapter_id");
    if (luid == nullptr) return invalid_argument;
    const icd::ThreadLockHold  held(lock_);
    const VendorAdapter *const asked = adapter(gpu);
    if (asked == nullptr) return opened_ == 0u ? not_initialized : expected_physical_gpu_handle;
    auto *const bytes = static_cast<uint8_t *>(luid);
    for (uint32_t index = 0u; index < sizeof(uint64_t); ++index)
        bytes[index] = static_cast<uint8_t>((asked->luid() >> (8u * index)) & 0xFFu);
    return ok;
}

int NvapiRoot::driver_and_branch_version(uint32_t *version, char *branch)
{
    AV_SCOPE("NvapiRoot::driver_and_branch_version");
    if (version == nullptr || branch == nullptr) return invalid_argument;
    const icd::ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    /* A second initialise that found no NVIDIA GPU leaves the first's open
     * over none, which NVAPI answers as a machine without one. */
    const VendorAdapter *const first = adapters_.nth_of(nvidia_vendor, 0u);
    if (first == nullptr) return device_not_found;
    const uint64_t full = first->driver_version();
    *version = static_cast<uint32_t>(((full >> 32u) & 0xFFFFu) * 100u + ((full >> 16u) & 0xFFFFu) % 100u);
    TextBuffer text(branch, short_string);
    text.append("avgpu ");
    first->append_driver_version(text);
    return ok;
}

int NvapiRoot::enum_display(uint32_t index, void **display)
{
    AV_SCOPE("NvapiRoot::enum_display");
    if (display == nullptr) return invalid_argument;
    const icd::ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    const uint32_t count = adapters_.count_of(nvidia_vendor);
    uint32_t       seen = 0u;
    for (uint32_t at = 0u; at < count; ++at) {
        if (!adapters_.nth_of(nvidia_vendor, at)->primary() || seen++ != index) continue;
        *display = &displays_[at];
        return ok;
    }
    return end_enumeration;
}

int NvapiRoot::gpus_from_display(const void *display, void **gpus, uint32_t *count)
{
    AV_SCOPE("NvapiRoot::gpus_from_display");
    if (gpus == nullptr || count == nullptr) return invalid_argument;
    const icd::ThreadLockHold held(lock_);
    if (opened_ == 0u) return not_initialized;
    const uint32_t found = adapters_.count_of(nvidia_vendor);
    for (uint32_t at = 0u; at < found; ++at) {
        const VendorAdapter *const driving = adapters_.nth_of(nvidia_vendor, at);
        if (display != &displays_[at] || !driving->primary()) continue;
        gpus[0] = const_cast<VendorAdapter *>(driving);
        *count = 1u;
        return ok;
    }
    return expected_display_handle;
}

const VendorAdapter *NvapiRoot::adapter(const void *gpu) const
{
    AV_SCOPE("NvapiRoot::adapter");
    if (opened_ == 0u || gpu == nullptr) return nullptr;
    const uint32_t count = adapters_.count_of(nvidia_vendor);
    for (uint32_t index = 0u; index < count; ++index) {
        const VendorAdapter *const candidate = adapters_.nth_of(nvidia_vendor, index);
        if (candidate == gpu) return candidate;
    }
    return nullptr;
}

}  // namespace av::vendor
