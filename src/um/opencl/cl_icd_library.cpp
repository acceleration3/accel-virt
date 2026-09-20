#include "um/opencl/cl_icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_icd_binding.hpp"

#include <windows.h>

#include <CL/cl_ext.h>

namespace av {

/* The one-shot's callback. An ABI boundary C requires: InitOnceExecuteOnce
 * takes a bare function and one argument, so the object is the argument and
 * this forwards straight into it. */
extern "C" BOOL WINAPI rvkcl_construct_once(PINIT_ONCE, PVOID library, PVOID *)
{
    static_cast<ClIcdLibrary *>(library)->construct();
    return TRUE;
}

ClIcdLibrary::Seams::Seams()
    : heap(), lock(), send_lock(), log(L"rvkcl"), kmt(), watcher(),
      root(heap, kmt, lock, send_lock, log, watcher, clicd::cl_icd_binding), trace()
{
    AV_SCOPE("ClIcdLibrary::Seams::Seams");
    log.open();
}

void ClIcdLibrary::construct()
{
    AV_SCOPE("ClIcdLibrary::construct");
    if (seams_.construct() != Status::ok) return;
    if (seams_.object().kmt.initialize() != Status::ok) {
        seams_.object().log.write("rvkcl: no D3DKMT wait event, refusing the loader");
        seams_.destroy();
        return;
    }
    Seams &seams = seams_.object();
    const Status traced = seams.trace.start("rvkcl", seams.root, crashed_);
    if (traced != Status::ok) seams.log.write("rvkcl: tracing is off: its section, pin or service failed");
}

clicd::ClIcdRoot *ClIcdLibrary::root()
{
    AV_SCOPE("ClIcdLibrary::root");
    if (!InitOnceExecuteOnce(reinterpret_cast<PINIT_ONCE>(&once_), &rvkcl_construct_once, this, nullptr))
        return nullptr;
    return seams_.constructed() ? &seams_.object().root : nullptr;
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
    return seams_.constructed() ? seams_.object().trace.crashed(pointers) : 0;
}

}  // namespace av
