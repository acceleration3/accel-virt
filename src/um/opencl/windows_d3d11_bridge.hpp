/* D3d11Bridge over COM (docs/d3d11-sharing.md): the application's own
 * Direct3D 11 device and its immediate context do every copy, through a
 * staging resource made for the one subresource and released after.
 *
 * The immediate context is the application's too, so each use of it is made
 * inside the device's ID3D10Multithread section, which an application that
 * asked for multithread protection enters around each of its own calls. The
 * section is held for the context's calls alone - the copy and the map, the
 * unmap and the copy back - and not while OpenCL moves the mapped bytes,
 * which may take as long as a frame. */
#ifndef AV_WINDOWS_D3D11_BRIDGE_HPP
#define AV_WINDOWS_D3D11_BRIDGE_HPP

#include "icd/opencl/d3d11_bridge.hpp"

/* d3d10_1.h before anything that includes d3d10.h, as it requires. */
#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <stdint.h>

namespace av {

class Kmt;

class WindowsD3d11Bridge final : public clicd::D3d11Bridge {
public:
    /* `kmt` reads back what a twin was published as. */
    explicit WindowsD3d11Bridge(Kmt &kmt);

    bool   device(void *object) override;
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
    /* `object` as an ID3D11Device or ID3D11Resource, with a reference the
     * caller releases; null for an object that is not one. */
    ID3D11Device   *as_device(void *object);
    ID3D11Resource *as_resource(void *object);
    /* Whether `resource` was made on `device`. */
    bool            made_on(ID3D11Resource &resource, ID3D11Device &device);
    /* What a buffer, a 2D texture or a 3D texture says of itself. */
    clicd::D3d11Resource description(ID3D11Resource &resource);
    /* A staging resource holding one subresource of `resource`, `cpu_access`
     * as D3D11_CPU_ACCESS_* says; null when the device will not make one. */
    ID3D11Resource *staging_for(ID3D11Device &device, ID3D11Resource &resource, uint32_t subresource,
                                UINT cpu_access);
    ID3D11Resource *staging_texture(ID3D11Device &device, ID3D11Resource &resource, uint32_t subresource,
                                    UINT cpu_access);
    /* Every command on `context` before this, run: a texel of `twin` read
     * through a staging texture, whose map waits for them. */
    cl_int             drain(ID3D11Device &device, ID3D11DeviceContext &context, ID3D11Resource &twin);
    /* The device's multithread section, or null for a device that has none. */
    ID3D10Multithread *section_of(ID3D11Device &device);
    void               enter(ID3D10Multithread *section);
    void               leave(ID3D10Multithread *section);
    /* `staging`, mapped as `type`, handed to `rows`, unmapped. The section is
     * left while `rows` works and entered again for the unmap. */
    cl_int             visit_mapped(ID3D11DeviceContext &context, ID3D10Multithread *section,
                                    ID3D11Resource &staging, D3D11_MAP type, clicd::D3d11Rows &rows);

    Kmt &kmt_;
    IID  device_iid_;
    IID  resource_iid_;
    IID  multithread_iid_;
};

}  // namespace av

#endif  // AV_WINDOWS_D3D11_BRIDGE_HPP
