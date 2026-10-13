/* One cl_mem made over a Direct3D 11 resource's subresource
 * (docs/d3d11-sharing.md): which resource and subresource, what the resource
 * is, which ways its bytes cross at acquire and release, and whether OpenCL
 * has it now. The cl_mem itself is an ordinary host buffer or image the ICD
 * made; this is what makes it the resource's. */
#ifndef AV_CLICD_D3D11_OBJECT_HPP
#define AV_CLICD_D3D11_OBJECT_HPP

#include "icd/opencl/d3d11_resource.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class D3d11Object {
public:
    /* `mem` is the application's cl_mem, `flags` what it was made with. */
    D3d11Object(const void *mem, void *resource, uint32_t subresource, const D3d11Resource &described,
                cl_mem_flags flags);
    D3d11Object(const D3d11Object &) = delete;
    D3d11Object &operator=(const D3d11Object &) = delete;

    bool                 is(const void *mem) const;
    bool                 over(const void *resource, uint32_t subresource) const;
    void                *resource() const;
    uint32_t             subresource() const;
    const D3d11Resource &described() const;
    /* Whether it is a buffer's, moved as bytes rather than as an image's rows. */
    bool                 buffer() const;
    /* What moves: a buffer's bytes, or the subresource's width, height and
     * depth in texels, as an OpenCL region names them. */
    size_t               bytes() const;
    void                 region(size_t *extent) const;
    /* Whether an acquire copies the resource in: not for an object OpenCL
     * only writes, whose bytes it is never shown. */
    bool                 reads_resource() const;
    /* Whether a release copies it back: not for one OpenCL only reads. */
    bool                 writes_resource() const;
    /* The shared texture the cl_mem is an image over, which acquire and
     * release copy through on the GPU (docs/d3d11-sharing.md, stage 2); null
     * for an object whose bytes cross through guest memory. */
    void                *twin() const;
    void                 pair(void *twin);
    bool                 acquired() const;
    void                 acquire();
    void                 release();

private:
    const void   *mem_;
    void         *resource_;
    uint32_t      subresource_;
    D3d11Resource described_;
    cl_mem_flags  flags_;
    bool          acquired_;
    void         *twin_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_D3D11_OBJECT_HPP
