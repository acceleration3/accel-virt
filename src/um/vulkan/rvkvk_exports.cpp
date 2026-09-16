/* rvkvk.dll's entry points (contracts/loader_interface.md): the two exports the
 * Windows Vulkan loader calls by name, each one line into the library in this
 * translation unit's storage - the Vulkan ICD's process-lifetime state
 * (constitution III) - and a DLL entry that does nothing at attach or detach,
 * where the loader lock allows nothing. No C runtime runs constructors, so the
 * storage is constinit and the library constructs itself on the first call.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * define them, as for rvktest.exe (program_entry.cpp). */
#include "core/trace/function_scope.hpp"
#include "um/vulkan/icd_library.hpp"

#include <windows.h>

static long WINAPI rvkvk_crashed(void *pointers);

static constinit av::IcdLibrary rvkvk_library{&rvkvk_crashed};

/* The process's unhandled-exception filter while rvkvk traces it. */
static long WINAPI rvkvk_crashed(void *pointers)
{
    AV_SCOPE("rvkvk_crashed");
    return rvkvk_library.crashed(pointers);
}

extern "C" BOOL WINAPI rvkvk_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pVersion)
{ return rvkvk_library.negotiate(pVersion); }

extern "C" PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char *pName)
{ return rvkvk_library.instance_proc_addr(instance, pName); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" int _fltused = 0;
