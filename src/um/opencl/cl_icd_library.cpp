#include "um/opencl/cl_icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_icd_binding.hpp"

#include <CL/cl_ext.h>

namespace av {

ClIcdLibrary::Seams::Seams()
    : heap(), lock(), send_lock(), log(L"avcl"), kmt(), watcher(), gl_share(),
      root(heap, kmt, lock, send_lock, log, watcher, gl_share, clicd::cl_icd_binding), trace()
{
    AV_SCOPE("ClIcdLibrary::Seams::Seams");
    log.open();
}

Status ClIcdLibrary::Seams::initialize()
{
    AV_SCOPE("ClIcdLibrary::Seams::initialize");
    return kmt.initialize();
}

clicd::ClIcdRoot *ClIcdLibrary::root()
{
    AV_SCOPE("ClIcdLibrary::root");
    Seams *const seams = driver_.seams();
    return seams == nullptr ? nullptr : &seams->root;
}

cl_int ClIcdLibrary::platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms)
{
    AV_SCOPE("ClIcdLibrary::platform_ids");
    clicd::ClIcdRoot *const answering = root();
    return answering != nullptr ? answering->platform_ids(num_entries, platforms, num_platforms)
                                : CL_PLATFORM_NOT_FOUND_KHR;
}

void *ClIcdLibrary::extension_function(const char *name)
{
    AV_SCOPE("ClIcdLibrary::extension_function");
    clicd::ClIcdRoot *const answering = root();
    return answering != nullptr ? answering->extension_function(name) : nullptr;
}

long ClIcdLibrary::crashed(void *pointers)
{
    AV_SCOPE("ClIcdLibrary::crashed");
    return driver_.crashed(pointers);
}

}  // namespace av
