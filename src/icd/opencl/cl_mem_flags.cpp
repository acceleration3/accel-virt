#include "icd/opencl/cl_mem_flags.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClMemFlags::ClMemFlags(cl_mem_flags flags) : flags_(flags)
{
    AV_SCOPE("ClMemFlags::ClMemFlags");
}

unsigned ClMemFlags::bits_of(cl_mem_flags flags)
{
    AV_SCOPE("ClMemFlags::bits_of");
    unsigned count = 0u;
    for (; flags != 0u; flags &= flags - 1u) ++count;
    return count;
}

cl_int ClMemFlags::judge() const
{
    AV_SCOPE("ClMemFlags::judge");
    if ((flags_ >> 32u) != 0u) return CL_INVALID_VALUE;
    if (bits_of(flags_ & device_access) > 1u || bits_of(flags_ & host_access) > 1u) return CL_INVALID_VALUE;
    if ((flags_ & CL_MEM_USE_HOST_PTR) != 0u && (flags_ & (CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR)) != 0u)
        return CL_INVALID_VALUE;
    return CL_SUCCESS;
}

cl_mem_flags ClMemFlags::for_host() const
{
    AV_SCOPE("ClMemFlags::for_host");
    return flags_ & ~host_access;
}

cl_mem_flags ClMemFlags::asked() const
{
    AV_SCOPE("ClMemFlags::asked");
    return flags_;
}

bool ClMemFlags::lets_host_read() const
{
    AV_SCOPE("ClMemFlags::lets_host_read");
    return (flags_ & (CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_NO_ACCESS)) == 0u;
}

bool ClMemFlags::lets_host_write() const
{
    AV_SCOPE("ClMemFlags::lets_host_write");
    return (flags_ & (CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS)) == 0u;
}

}
