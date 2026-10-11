#include "icd/opencl/d3d11_object.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

D3d11Object::D3d11Object(const void *mem, void *resource, uint32_t subresource, const D3d11Resource &described,
                         cl_mem_flags flags)
    : mem_(mem), resource_(resource), subresource_(subresource), described_(described), flags_(flags),
      acquired_(false)
{
    AV_SCOPE("D3d11Object::D3d11Object");
}

bool D3d11Object::is(const void *mem) const
{
    AV_SCOPE("D3d11Object::is");
    return mem != nullptr && mem == mem_;
}

bool D3d11Object::over(const void *resource, uint32_t subresource) const
{
    AV_SCOPE("D3d11Object::over");
    return resource == resource_ && subresource == subresource_;
}

void *D3d11Object::resource() const
{
    AV_SCOPE("D3d11Object::resource");
    return resource_;
}

uint32_t D3d11Object::subresource() const
{
    AV_SCOPE("D3d11Object::subresource");
    return subresource_;
}

const D3d11Resource &D3d11Object::described() const
{
    AV_SCOPE("D3d11Object::described");
    return described_;
}

bool D3d11Object::buffer() const
{
    AV_SCOPE("D3d11Object::buffer");
    return described_.kind() == D3d11Resource::Kind::buffer;
}

size_t D3d11Object::bytes() const
{
    AV_SCOPE("D3d11Object::bytes");
    return static_cast<size_t>(described_.bytes());
}

void D3d11Object::region(size_t *extent) const
{
    AV_SCOPE("D3d11Object::region");
    extent[0] = described_.width_of(subresource_);
    extent[1] = described_.height_of(subresource_);
    extent[2] = described_.depth_of(subresource_);
}

bool D3d11Object::reads_resource() const
{
    AV_SCOPE("D3d11Object::reads_resource");
    return (flags_ & CL_MEM_WRITE_ONLY) == 0u;
}

bool D3d11Object::writes_resource() const
{
    AV_SCOPE("D3d11Object::writes_resource");
    return (flags_ & CL_MEM_READ_ONLY) == 0u;
}

bool D3d11Object::acquired() const
{
    AV_SCOPE("D3d11Object::acquired");
    return acquired_;
}

void D3d11Object::acquire()
{
    AV_SCOPE("D3d11Object::acquire");
    acquired_ = true;
}

void D3d11Object::release()
{
    AV_SCOPE("D3d11Object::release");
    acquired_ = false;
}

}  // namespace av::clicd
