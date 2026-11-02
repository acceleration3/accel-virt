#include "core/trace/function_scope.hpp"
#include "um/opencl/cl_icd_library.hpp"

#include <windows.h>

#include <CL/cl_ext.h>

static long WINAPI avcl_crashed(void *pointers);

static constinit av::ClIcdLibrary avcl_library{&avcl_crashed};

static long WINAPI avcl_crashed(void *pointers)
{
    AV_SCOPE("avcl_crashed");
    return avcl_library.crashed(pointers);
}

extern "C" BOOL WINAPI avcl_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" CL_API_ENTRY void *CL_API_CALL clGetExtensionFunctionAddress(const char *func_name)
{ return avcl_library.extension_function(func_name); }

extern "C" CL_API_ENTRY cl_int CL_API_CALL clIcdGetPlatformIDsKHR(cl_uint num_entries,
                                                                  cl_platform_id *platforms,
                                                                  cl_uint *num_platforms)
{ return avcl_library.platform_ids(num_entries, platforms, num_platforms); }
