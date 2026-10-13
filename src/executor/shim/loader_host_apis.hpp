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

#include "executor/host_apis.hpp"
#include "executor/host_gpu_set.hpp"
#include "executor/shim/compositor_device.hpp"
#include "executor/shim/d3d11_host_device.hpp"
#include "executor/shim/d3d12_host_device.hpp"
#include "executor/shim/d3d9_host_device.hpp"
#include "executor/shim/d3d_runtime.hpp"
#include "core/log/driver_log.hpp"
#include "core/support/object_storage.hpp"
#include "device/host/stdio_log.hpp"
#include "executor/d3d/shared_surfaces.hpp"
#include "executor/sharing/host_shares.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/opencl_device.hpp"
#include "proto/context/device_uuid.hpp"

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
    int32_t  render_minor(uint32_t index) override;
    const GlDisplay    *gl_display(uint32_t index) override;
    const OpenClDevice *opencl_device(uint32_t index) override;
    D3d11HostDevice    *d3d11_device(uint32_t index) override;
    D3d12HostDevice    *d3d12_device(uint32_t index) override;
    D3d9HostDevice     *d3d9_device(uint32_t index) override;
    SharedSurfaces     *surfaces(uint32_t index) override;
    HostShares         *shares(uint32_t index) override;

private:
    /* Room for a driver's whole extension list in one call, because the
     * enumeration has no offset to continue from. Mesa and NVIDIA report
     * under two hundred each. */
    static constexpr uint32_t max_extensions = 512u;

    void enumerate();
    /* Which AV_BLOB_* kinds `device` can import, asked of its extensions
     * rather than assumed: a device without the host-pointer extension imports
     * nothing and the guest is told to copy. */
    uint32_t kinds_of(void *device);

    void    *instance_;
    DeviceUuid uuids_[HostGpuSet::max_gpus];
    uint32_t sub_apis_[HostGpuSet::max_gpus];
    uint32_t vulkan_kinds_[HostGpuSet::max_gpus];
    uint32_t opencl_kinds_[HostGpuSet::max_gpus];
    int32_t  render_minors_[HostGpuSet::max_gpus];
    /* OpenCL is served where a device on this host says it is the same GPU,
     * and OpenGL where a context made on a display answers that GPU's UUID -
     * because every cheaper answer, an EGL device count or a vendor string,
     * has been wrong on this host at least once. Both are kept once found. */
    OpenClDevice opencl_devices_[HostGpuSet::max_gpus];
    GlDisplay    gl_displays_[HostGpuSet::max_gpus];
    /* Direct3D is served where DXVK makes a device on the GPU, found and made
     * here for the reason the displays are: a device made inside a guest's
     * first batch would cost it dxgkrnl's timeout. */
    D3dRuntime      d3d_runtime_;
    D3d11HostDevice d3d11_devices_[HostGpuSet::max_gpus];
    /* Each GPU's compositing context, opened beside its Direct3D 11 device
     * and closed after it. */
    CompositorDevice compositors_[HostGpuSet::max_gpus];
    D3d12HostDevice d3d12_devices_[HostGpuSet::max_gpus];
    D3d9HostDevice  d3d9_devices_[HostGpuSet::max_gpus];
    SharedSurfaces  surfaces_[HostGpuSet::max_gpus];
    /* What every GPU's shares are kept in, and the shares, before the
     * devices whose textures they hold are closed and after they are made. */
    HostHeap                   heap_;
    /* QEMU's stderr, where what the shares see is written. */
    StdioLog                   share_sink_;
    DriverLog                  share_lines_;
    ObjectStorage<HostShares>  shares_[HostGpuSet::max_gpus];
    VkExtensionProperties extensions_[max_extensions];
    uint32_t count_;
    bool     opened_;
};

}  // namespace av

#endif  // AV_LOADER_HOST_APIS_HPP
