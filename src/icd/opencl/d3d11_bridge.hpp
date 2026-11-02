#ifndef AV_CLICD_D3D11_BRIDGE_HPP
#define AV_CLICD_D3D11_BRIDGE_HPP

#include <CL/cl.h>

#include <stdint.h>

namespace av::clicd {

class D3d11Resource;
class D3d11Rows;

class D3d11Bridge {
public:
    D3d11Bridge(const D3d11Bridge &) = delete;
    D3d11Bridge &operator=(const D3d11Bridge &) = delete;

    virtual bool   device(void *object) = 0;
    virtual bool   describe(void *object, void *device, D3d11Resource &described) = 0;
    virtual void   hold(void *object) = 0;
    virtual void   let_go(void *object) = 0;
    virtual cl_int read(void *device, void *resource, uint32_t subresource, D3d11Rows &rows) = 0;
    virtual cl_int write(void *device, void *resource, uint32_t subresource, D3d11Rows &rows) = 0;
    virtual void  *twin(void *device, void *resource, uint32_t subresource, uint32_t kmt_device, uint64_t &handle,
                        uint64_t &bytes) = 0;
    virtual void   close_handle(uint64_t handle) = 0;
    virtual cl_int to_twin(void *device, void *resource, uint32_t subresource, void *twin) = 0;
    virtual cl_int from_twin(void *device, void *twin, void *resource, uint32_t subresource) = 0;

protected:
    D3d11Bridge() = default;
    ~D3d11Bridge() = default;
};

}

#endif
