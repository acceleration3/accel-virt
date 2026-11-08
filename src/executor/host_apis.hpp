#ifndef AV_HOST_APIS_HPP
#define AV_HOST_APIS_HPP

#include <stdint.h>

namespace av {

class D3d11HostDevice;
class D3d12HostDevice;
class D3d9HostDevice;
class ClFreedMemory;
class GlDisplay;
class OpenClDevice;
class HostMemorySource;
class HostRamLedger;
class HostShares;
class VramLedger;
class SharedSurfaces;

class HostApis {
public:
    HostApis(const HostApis &) = delete;
    HostApis &operator=(const HostApis &) = delete;

    virtual uint32_t gpu_count() = 0;
    virtual void     gpu_uuid(uint32_t index, uint8_t *out) = 0;
    virtual uint32_t sub_apis(uint32_t index) = 0;
    virtual int32_t  render_minor(uint32_t index) = 0;

    virtual const GlDisplay    *gl_display(uint32_t index) = 0;
    virtual const OpenClDevice *opencl_device(uint32_t index) = 0;
    virtual D3d11HostDevice    *d3d11_device(uint32_t index) = 0;
    virtual D3d12HostDevice    *d3d12_device(uint32_t index) = 0;
    virtual D3d9HostDevice     *d3d9_device(uint32_t index) = 0;
    virtual SharedSurfaces     *surfaces(uint32_t index) = 0;
    virtual HostShares         *shares(uint32_t index) = 0;
    virtual HostMemorySource   *memory_source(uint32_t index) = 0;
    virtual VramLedger         *vram_ledger(uint32_t index) = 0;
    virtual ClFreedMemory      *cl_freed(uint32_t index) = 0;
    virtual HostRamLedger      *host_ram_ledger() = 0;

protected:
    HostApis() = default;
    ~HostApis() = default;
};

}

#endif
