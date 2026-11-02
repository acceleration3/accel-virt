#include "core/trace/function_scope.hpp"
#include "um/vulkan/icd_library.hpp"

#include <windows.h>

static long WINAPI avvk_crashed(void *pointers);

static constinit av::IcdLibrary avvk_library{&avvk_crashed};

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
