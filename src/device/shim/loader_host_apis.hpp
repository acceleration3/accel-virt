/* What this host's loaders say, asked through the loaders themselves.
 *
 * The other implementation of `HostApis`, and the reason that seam is virtual:
 * this one needs a machine with a GPU and the mock does not.
 *
 * A host with no GPU, no driver, or a Vulkan that will not make an instance is
 * not a failure here: it is a host that serves nothing, every query answers
 * zero, and the device then describes no capset rather than one it cannot
 * honour. */
#ifndef AV_LOADER_HOST_APIS_HPP
#define AV_LOADER_HOST_APIS_HPP

#include "device/executor/host_apis.hpp"
#include "device/executor/host_gpu_set.hpp"

#include <stdint.h>
#include <vulkan/vulkan_core.h>

namespace av {

class LoaderHostApis final : public HostApis {
public:
    LoaderHostApis();
    ~LoaderHostApis();

    /* Asks the host's Vulkan once. A host with no instance and no device is
     * not a failure: every query below then answers zero. */
    void open();
    void close();

    uint32_t gpu_count() override;
    void     gpu_uuid(uint32_t index, uint8_t *out) override;
    uint32_t sub_apis(uint32_t index) override;
    uint32_t import_kinds(uint32_t index, uint32_t sub_api) override;

private:
    static constexpr uint32_t uuid_bytes = 16u;
    /* Room for a driver's whole extension list in one call, because the
     * enumeration has no offset to continue from. Mesa and NVIDIA report
     * under two hundred each. */
    static constexpr uint32_t max_extensions = 512u;

    void enumerate();
    /* Which RVK_BLOB_* kinds `device` can import, asked of its extensions
     * rather than assumed: a device without the host-pointer extension imports
     * nothing and the guest is told to copy. */
    uint32_t kinds_of(void *device);

    void    *instance_;
    uint8_t  uuids_[HostGpuSet::max_gpus][uuid_bytes];
    uint32_t sub_apis_[HostGpuSet::max_gpus];
    uint32_t vulkan_kinds_[HostGpuSet::max_gpus];
    VkExtensionProperties extensions_[max_extensions];
    uint32_t count_;
    bool     opened_;
};

}  // namespace av

#endif  // AV_LOADER_HOST_APIS_HPP
