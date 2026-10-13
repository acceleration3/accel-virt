/* What only COM can do for cl_khr_d3d11_sharing (docs/d3d11-sharing.md): say
 * what a Direct3D 11 object is, hold a reference to one, and move a
 * subresource's bytes between the resource and guest memory through a staging
 * resource of the device's own. Portable code cannot include d3d11.h, so every
 * object here is the pointer the application handed over.
 *
 * Two implementations: the Windows one in the shim, over the application's
 * device and its immediate context, and one in the tests that keeps the bytes
 * of each resource it was told of. */
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

    /* Whether `object` is an ID3D11Device. */
    virtual bool   device(void *object) = 0;
    /* What `object` is, as a resource of `device`: false for an object that is
     * not an ID3D11Resource, or one another device made. */
    virtual bool   describe(void *object, void *device, D3d11Resource &described) = 0;
    /* One more reference to `object`, and one fewer: what a context holds its
     * device by, and a cl_mem its resource. */
    virtual void   hold(void *object) = 0;
    virtual void   let_go(void *object) = 0;
    /* `subresource` of `resource`, copied into a staging resource on `device`
     * once every command before it has run, mapped for reading and handed to
     * `rows`. */
    virtual cl_int read(void *device, void *resource, uint32_t subresource, D3d11Rows &rows) = 0;
    /* A staging resource on `device` mapped for writing, handed to `rows` to
     * fill, then copied into `subresource` of `resource` on the immediate
     * context, ahead of every command made on it afterwards. */
    virtual cl_int write(void *device, void *resource, uint32_t subresource, D3d11Rows &rows) = 0;
    /* A texture on `device` made to be shared, the size and format of
     * `subresource` of `resource`, with an NT handle to it in `handle` for the
     * caller to open and close (close_handle): what OpenCL opens as the image
     * itself (docs/d3d11-sharing.md, stage 2). Null, `handle` 0, when the
     * device will not share one - a format the host cannot export, or a
     * resource that is no 2D texture. Held once; let_go releases it. */
    virtual void  *twin(void *device, void *resource, uint32_t subresource, uint64_t &handle) = 0;
    virtual void   close_handle(uint64_t handle) = 0;
    /* `subresource` of `resource` copied into `twin`, and every Direct3D 11
     * command before the copy run, the copy included, when this returns. */
    virtual cl_int to_twin(void *device, void *resource, uint32_t subresource, void *twin) = 0;
    /* `twin` copied back into `subresource` of `resource` on the immediate
     * context, ahead of every command made on it afterwards. */
    virtual cl_int from_twin(void *device, void *twin, void *resource, uint32_t subresource) = 0;

protected:
    D3d11Bridge() = default;
    ~D3d11Bridge() = default;
};

}  // namespace av::clicd

#endif  // AV_CLICD_D3D11_BRIDGE_HPP
