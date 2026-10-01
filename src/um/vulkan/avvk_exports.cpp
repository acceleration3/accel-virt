/* avvk.dll's entry points (contracts/loader_interface.md): the two exports the
 * Windows Vulkan loader calls by name, each one line into the library in this
 * translation unit's storage - the Vulkan ICD's process-lifetime state
 * (constitution III) - and a DLL entry that does nothing at attach or detach,
 * where the loader lock allows nothing. No C runtime runs constructors, so the
 * storage is constinit and the library constructs itself on the first call.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * define them, as for avtest.exe (program_entry.cpp). */
#include "core/trace/function_scope.hpp"
#include "um/vulkan/icd_library.hpp"

#include <windows.h>

static long WINAPI avvk_crashed(void *pointers);

static constinit av::IcdLibrary avvk_library{&avvk_crashed};

/* The process's unhandled-exception filter while avvk traces it. */
static long WINAPI avvk_crashed(void *pointers)
{
    AV_SCOPE("avvk_crashed");
    return avvk_library.crashed(pointers);
}

extern "C" BOOL WINAPI avvk_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" VkResult VKAPI_CALL vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t *pVersion)
{ return avvk_library.negotiate(pVersion); }

extern "C" PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char *pName)
{ return avvk_library.instance_proc_addr(instance, pName); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
