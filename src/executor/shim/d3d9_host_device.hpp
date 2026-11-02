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

    [[nodiscard]] bool open(const D3dRuntime &runtime, const uint8_t *uuid);
    void               close();
    bool               opened() const;

    int32_t make_device(IDirect3DDevice9 *&device) const;
    int32_t caps(_D3DCAPS9 &caps) const;
    d3d9proto::FormatCaps9 format_caps(uint32_t format) const;
    StagedFrame *stage_frame(IDirect3DDevice9 *device, IDirect3DSurface9 *source, uint32_t width, uint32_t height,
                             uint32_t format, bool multisampled);

private:
    uint32_t adapter_for(const uint8_t *uuid);
    bool     checks(uint32_t usage, uint32_t type, uint32_t format) const;
    int32_t  multisample(uint32_t format, uint32_t type, uint32_t &quality_levels) const;

    IDirect3D9 *d3d9_;
    uint32_t    adapter_;
    D3d9FramePool frames_;
};

}

#endif
