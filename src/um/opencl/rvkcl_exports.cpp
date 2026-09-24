/* rvkcl.dll's entry points (contracts/icd_interface.md): the two names the
 * Khronos ICD loader may resolve, each one line into the library in this
 * translation unit's storage - the OpenCL ICD's process-lifetime state
 * (constitution III) - and a DLL entry that does nothing at attach or detach,
 * where the loader lock allows nothing. No C runtime runs constructors, so the
 * storage is constinit and the library constructs itself on the first call.
 *
 * The pinned loader resolves only clGetExtensionFunctionAddress by name and
 * asks it for clIcdGetPlatformIDsKHR (khrIcdVendorAdd, loader/icd.c:111-119);
 * older loaders looked the second up directly, and exporting it costs a line.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * define them, as for rvkvk.dll (rvkvk_exports.cpp). */
#include "core/trace/function_scope.hpp"
#include "um/opencl/cl_icd_library.hpp"

#include <windows.h>

#include <CL/cl_ext.h>

static long WINAPI rvkcl_crashed(void *pointers);

static constinit av::ClIcdLibrary rvkcl_library{&rvkcl_crashed};

/* The process's unhandled-exception filter while rvkcl traces it. */
static long WINAPI rvkcl_crashed(void *pointers)
{
    AV_SCOPE("rvkcl_crashed");
    return rvkcl_library.crashed(pointers);
}

extern "C" BOOL WINAPI rvkcl_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" CL_API_ENTRY void *CL_API_CALL clGetExtensionFunctionAddress(const char *func_name)
{ return rvkcl_library.extension_function(func_name); }

extern "C" CL_API_ENTRY cl_int CL_API_CALL clIcdGetPlatformIDsKHR(cl_uint num_entries,
                                                                  cl_platform_id *platforms,
                                                                  cl_uint *num_platforms)
{ return rvkcl_library.platform_ids(num_entries, platforms, num_platforms); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
