#ifndef AV_CLICD_D3D11_OBJECT_HPP
#define AV_CLICD_D3D11_OBJECT_HPP

#include "icd/opencl/d3d11_resource.hpp"

#include <CL/cl.h>

#include <stddef.h>
#include <stdint.h>

namespace av::clicd {

class D3d11Object {
public:
    D3d11Object(const void *mem, void *resource, uint32_t subresource, const D3d11Resource &described,
                cl_mem_flags flags);
    D3d11Object(const D3d11Object &) = delete;
    D3d11Object &operator=(const D3d11Object &) = delete;

    bool                 is(const void *mem) const;
    bool                 over(const void *resource, uint32_t subresource) const;
    void                *resource() const;
    uint32_t             subresource() const;
    const D3d11Resource &described() const;
    bool                 buffer() const;
    size_t               bytes() const;
    void                 region(size_t *extent) const;
    bool                 reads_resource() const;
    bool                 writes_resource() const;
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

}

#endif
