/* What this host's loaders say, asked through the loaders themselves.
 *
 * The other implementation of `HostApis`, and the reason that seam is virtual:
 * this one needs a machine with a GPU and the mock does not.
 *
 * Everything is resolved at run time rather than linked, so the library loads
 * on a host with no Vulkan at all and answers that it serves nothing - which
 * is what the device needs to hear before it describes a capset. */
#ifndef AV_LOADER_HOST_APIS_HPP
#define AV_LOADER_HOST_APIS_HPP

#include "device/executor/host_apis.hpp"
#include "device/executor/host_gpu_set.hpp"

#include <stdint.h>

namespace av {

class LoaderHostApis final : public HostApis {
public:
    LoaderHostApis();
    ~LoaderHostApis();

    /* Opens the host's Vulkan loader and enumerates. A host with no loader, no
     * instance or no device is not a failure: it is a host that serves
     * nothing, and every query below then answers zero. */
    void open();
    void close();

    uint32_t gpu_count() override;
    void     gpu_uuid(uint32_t index, uint8_t *out) override;
    uint32_t sub_apis(uint32_t index) override;
    uint32_t import_kinds(uint32_t index, uint32_t sub_api) override;

private:
    static constexpr uint32_t uuid_bytes = 16u;

    void enumerate();
    /* Which RVK_BLOB_* kinds `device` can import, asked of its extensions
     * rather than assumed: a device without the host-pointer extension imports
     * nothing and the guest is told to copy. */
    uint32_t kinds_of(void *device);

    void    *library_;
    void    *instance_;
    uint8_t  uuids_[HostGpuSet::max_gpus][uuid_bytes];
    uint32_t sub_apis_[HostGpuSet::max_gpus];
    uint32_t vulkan_kinds_[HostGpuSet::max_gpus];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_LOADER_HOST_APIS_HPP
