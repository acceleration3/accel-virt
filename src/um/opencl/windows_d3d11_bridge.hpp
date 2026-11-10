#ifndef AV_WINDOWS_D3D11_BRIDGE_HPP
#define AV_WINDOWS_D3D11_BRIDGE_HPP

#include "icd/opencl/d3d11_bridge.hpp"
#include "um/opencl/windows_d3d10_path.hpp"

#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <stdint.h>

namespace av {

class Kmt;

class WindowsD3d11Bridge final : public clicd::D3d11Bridge {
public:
    explicit WindowsD3d11Bridge(Kmt &kmt);

    bool   device(void *object) override;
    bool   d3d10_device(void *object) override;
    bool   describe(void *object, void *device, clicd::D3d11Resource &described) override;
    void   hold(void *object) override;
    void   let_go(void *object) override;
    cl_int read(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows) override;
    cl_int write(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows) override;
    void  *twin(void *device, void *resource, uint32_t subresource, uint32_t kmt_device, uint64_t &handle,
                uint64_t &bytes) override;
    void   close_handle(uint64_t handle) override;
    cl_int to_twin(void *device, void *resource, uint32_t subresource, void *twin) override;
    cl_int from_twin(void *device, void *twin, void *resource, uint32_t subresource) override;

private:
    ID3D11Device   *as_device(void *object);
    ID3D11Resource *as_resource(void *object);
    bool            made_on(ID3D11Resource &resource, ID3D11Device &device);
    clicd::D3d11Resource description(ID3D11Resource &resource);
    ID3D11Resource *staging_for(ID3D11Device &device, ID3D11Resource &resource, uint32_t subresource,
                                UINT cpu_access);
    ID3D11Resource *staging_texture(ID3D11Device &device, ID3D11Resource &resource, uint32_t subresource,
                                    UINT cpu_access);
    cl_int             drain(ID3D11Device &device, ID3D11DeviceContext &context, ID3D11Resource &twin);
    ID3D10Multithread *section_of(ID3D11Device &device);
    void               enter(ID3D10Multithread *section);
    void               leave(ID3D10Multithread *section);
    cl_int             visit_mapped(ID3D11DeviceContext &context, ID3D10Multithread *section,
                                    ID3D11Resource &staging, D3D11_MAP type, clicd::D3d11Rows &rows);

    Kmt                &kmt_;
    WindowsD3d10Path    ten_;
    IID  device_iid_;
    IID  resource_iid_;
    IID  multithread_iid_;
};

}

#endif
