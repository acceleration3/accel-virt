#ifndef AV_WINDOWS_D3D10_PATH_HPP
#define AV_WINDOWS_D3D10_PATH_HPP

#include "icd/opencl/d3d11_bridge.hpp"

#include <d3d10_1.h>
#include <dxgi1_2.h>

#include <stdint.h>

namespace av {

class WindowsD3d10Path {
public:
    WindowsD3d10Path();

    bool   device(void *object);
    bool   describe(void *object, void *device, clicd::D3d11Resource &described);
    cl_int read(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows);
    cl_int write(void *device, void *resource, uint32_t subresource, clicd::D3d11Rows &rows);

private:
    ID3D10Device   *as_device(void *object);
    ID3D10Resource *as_resource(void *object);
    bool            made_on(ID3D10Resource &resource, ID3D10Device &device);
    clicd::D3d11Resource description(ID3D10Resource &resource);
    ID3D10Resource *staging_for(ID3D10Device &device, ID3D10Resource &resource, uint32_t subresource,
                                UINT cpu_access);
    cl_int          visit_mapped(ID3D10Resource &staging, ID3D10Multithread *section, D3D10_MAP type,
                                 clicd::D3d11Rows &rows);
    ID3D10Multithread *section_of(ID3D10Device &device);
    void               enter(ID3D10Multithread *section);
    void               leave(ID3D10Multithread *section);

    IID device_iid_;
    IID resource_iid_;
    IID multithread_iid_;
};

}

#endif
