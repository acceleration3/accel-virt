/* One host GPU's Direct3D 9: DXVK's IDirect3D9 and the adapter ordinal that
 * is this GPU, which every Direct3D 9 device a session makes on it comes from.
 *
 * Unlike Direct3D 11 there is nothing to share between sessions: a Direct3D 9
 * device's state is all of it its own, so each guest device gets a host
 * device of its own (make_device). The devices are plain Direct3D 9, not 9Ex,
 * because 9Ex refuses D3DPOOL_MANAGED, and managed is what lets the host
 * write a texture the guest locks without a staging copy of its own. None has
 * a window: what a device draws reaches the screen through the guest's
 * presents, never through a host swap chain, and DXVK's d3d9 presents nothing
 * for a device whose window is null. */
#ifndef AV_D3D9_HOST_DEVICE_HPP
#define AV_D3D9_HOST_DEVICE_HPP

#include "executor/shim/d3d9_frame_pool.hpp"
#include "proto/generated/d3d9/d3d9_wire_types.h"

#include <stdint.h>

struct _D3DCAPS9;
struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DSurface9;

namespace av {

class D3dRuntime;
class StagedFrame;

class D3d9HostDevice {
public:
    D3d9HostDevice();
    ~D3d9HostDevice();
    D3d9HostDevice(const D3d9HostDevice &) = delete;
    D3d9HostDevice &operator=(const D3d9HostDevice &) = delete;

    /* Direct3D 9 on the GPU `uuid` names. False where `runtime` has no d3d9,
     * or none of its adapters is that GPU. */
    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid);
    void               close();
    bool               opened() const;

    /* A device of its own on this GPU, with a reference the caller owns; the
     * HRESULT, `device` null on failure. */
    int32_t make_device(IDirect3DDevice9 *&device) const;
    /* What IDirect3D9 says of the adapter; the HRESULT. */
    int32_t caps(_D3DCAPS9 &caps) const;
    /* QueryCaps9's row for `format` (d3d9.toml's FormatCaps9): what it can
     * be, the multisample types it renders at windowed and their quality
     * levels. */
    d3d9proto::FormatCaps9 format_caps(uint32_t format) const;
    /* A present's render target staged for the present engine
     * (D3d9FramePool); null when none could be. */
    StagedFrame *stage_frame(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                             uint32_t format, bool multisampled);

private:
    /* The ordinal of the adapter whose Vulkan device has this UUID; ~0u for none. */
    uint32_t adapter_for(const uint8_t *uuid);
    bool     checks(uint32_t usage, uint32_t type, uint32_t format) const;
    /* CheckDeviceMultiSampleType, windowed; the HRESULT. */
    int32_t  multisample(uint32_t format, uint32_t type, uint32_t &quality_levels) const;

    IDirect3D9 *d3d9_;
    uint32_t    adapter_;
    D3d9FramePool frames_;
};

}  // namespace av

#endif  // AV_D3D9_HOST_DEVICE_HPP
