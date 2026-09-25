/* What this host's own graphics loaders say they can do.
 *
 * The executor library is a shared object of its own: it cannot ask the device
 * what GPUs exist, because the device is on the other side of a C ABI that
 * carries one command's payload and nothing else. So it asks the loaders
 * itself, and this is the seam where that happens - virtual because there are
 * two real implementations, the loaders and the mock that makes a host with no
 * GPU, a host with two, and a host whose OpenCL cannot import testable on a
 * machine that is none of those (feature 042).
 *
 * Sub-API and import-kind values are the ABI's own: RVK_EXECUTOR_SUB_API_* and
 * RVK_BLOB_*. This header names no Vulkan, EGL or OpenCL type, so the classes
 * above it compile on a machine with no loader at all; the display and device a
 * context runs on are the shim's classes, declared and never opened here. */
#ifndef AV_HOST_APIS_HPP
#define AV_HOST_APIS_HPP

#include <stdint.h>

namespace av {

class D3d11HostDevice;
class D3d12HostDevice;
class GlDisplay;
class OpenClDevice;

class HostApis {
public:
    HostApis(const HostApis &) = delete;
    HostApis &operator=(const HostApis &) = delete;

    /* How many GPUs this host's Vulkan loader enumerates. Zero is an answer, not
     * a failure: a host with no Vulkan serves no sub-API and says so. */
    virtual uint32_t gpu_count() = 0;
    /* The 16 bytes of `index`'s VkPhysicalDeviceIDProperties::deviceUUID, which
     * is what the device names a GPU by. Untouched for an index nobody has. */
    virtual void     gpu_uuid(uint32_t index, uint8_t *out) = 0;
    /* The RVK_EXECUTOR_SUB_API_* bits this GPU can actually run. */
    virtual uint32_t sub_apis(uint32_t index) = 0;
    /* The RVK_BLOB_* kinds `sub_api` can import on this GPU, or zero for "copy
     * at the boundary". Measured per GPU and per API because the answers
     * differ: a host pointer into Vulkan works where a dma-buf into OpenCL
     * does not (feature 042, research). */
    virtual uint32_t import_kinds(uint32_t index, uint32_t sub_api) = 0;
    /* The minor number of this GPU's DRM render node, or -1 where the driver
     * does not say (VK_EXT_physical_device_drm). */
    virtual int32_t  render_minor(uint32_t index) = 0;

    /* What a context on `index` runs on, found when the host was asked and
     * kept for as long as this is: the EGL display an OpenGL context is made
     * on, and the OpenCL platform and device a context is shown. Null for a GPU
     * with none. Read from a worker when a context opens, and never changed
     * after the host was asked, so they need no lock (feature 044, T034/T035). */
    virtual const GlDisplay    *gl_display(uint32_t index) = 0;
    virtual const OpenClDevice *opencl_device(uint32_t index) = 0;
    /* The GPU's Direct3D 11 device, made through DXVK when the host was asked
     * and shared by every Direct3D session on that GPU; null for a GPU DXVK
     * does not run on, or a host without it. Not const: sessions take its
     * lock and share resources through it. */
    virtual D3d11HostDevice    *d3d11_device(uint32_t index) = 0;
    /* The GPU's Direct3D 12 device, made through vkd3d-proton alongside it;
     * null for a GPU with none. */
    virtual D3d12HostDevice    *d3d12_device(uint32_t index) = 0;

protected:
    HostApis() = default;
    /* Not virtual: nothing here is destroyed through this pointer, and a
     * deleting destructor would need an operator delete this build has not
     * got (CLAUDE.md, Adaptations). */
    ~HostApis() = default;
};

}  // namespace av

#endif  // AV_HOST_APIS_HPP
