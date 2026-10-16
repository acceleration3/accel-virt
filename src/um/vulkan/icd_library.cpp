#include "um/vulkan/icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/icd_binding.hpp"

namespace av {

IcdLibrary::Seams::Seams()
    : process(L"avvk"), lock(), send_lock(), memory_lock(), windows(), chains_lock(),
      chains(process.heap(), process.kmt(), chains_lock, process.log()),
      root(process.heap(), process.kmt(), lock, send_lock, memory_lock, process.log(), icd::icd_binding, windows, chains)
{
    AV_SCOPE("IcdLibrary::Seams::Seams");
}

VkResult IcdLibrary::negotiate(uint32_t *version)
{
    AV_SCOPE("IcdLibrary::negotiate");
    icd::IcdRoot *const answering = driver_.root();
    return answering != nullptr ? answering->negotiate(version) : VK_ERROR_INCOMPATIBLE_DRIVER;
}

PFN_vkVoidFunction IcdLibrary::instance_proc_addr(VkInstance instance, const char *name)
{
    AV_SCOPE("IcdLibrary::instance_proc_addr");
    icd::IcdRoot *const answering = driver_.root();
    return answering != nullptr ? answering->instance_proc_addr(instance, name) : nullptr;
}

long IcdLibrary::crashed(void *pointers)
{
    AV_SCOPE("IcdLibrary::crashed");
    return driver_.crashed(pointers);
}

}  // namespace av
