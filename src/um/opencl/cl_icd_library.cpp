#include "um/opencl/cl_icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_icd_binding.hpp"

#include <CL/cl_ext.h>

namespace av {

ClIcdLibrary::Seams::Seams()
    : process(L"avcl", "avcl: "), lock(), send_lock(), objects_lock(), blocks_lock(), watcher(), gl_share(), d3d_adapters(),
      d3d11(process.kmt()), root(process.heap(), process.kmt(), lock, send_lock, objects_lock, blocks_lock, process.log(), watcher,
                    gl_share, d3d_adapters, d3d11, clicd::cl_icd_binding)
{
    AV_SCOPE("ClIcdLibrary::Seams::Seams");
}

cl_int ClIcdLibrary::platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms)
{
    AV_SCOPE("ClIcdLibrary::platform_ids");
    clicd::ClIcdRoot *const answering = driver_.root();
    return answering != nullptr ? answering->platform_ids(num_entries, platforms, num_platforms)
                                : CL_PLATFORM_NOT_FOUND_KHR;
}

void *ClIcdLibrary::extension_function(const char *name)
{
    AV_SCOPE("ClIcdLibrary::extension_function");
    clicd::ClIcdRoot *const answering = driver_.root();
    return answering != nullptr ? answering->extension_function(name) : nullptr;
}

long ClIcdLibrary::crashed(void *pointers)
{
    AV_SCOPE("ClIcdLibrary::crashed");
    return driver_.crashed(pointers);
}

}  // namespace av
