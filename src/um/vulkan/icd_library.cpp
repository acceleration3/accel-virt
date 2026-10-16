#include "um/vulkan/icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/icd_binding.hpp"

namespace av {

IcdLibrary::Seams::Seams()
    : heap(), lock(), send_lock(), memory_lock(), log(L"avvk"), kmt(), windows(), chains_lock(),
      chains(heap, kmt, chains_lock, log), root(heap, kmt, lock, send_lock, memory_lock, log, icd::icd_binding, windows, chains),
      trace()
{
    AV_SCOPE("IcdLibrary::Seams::Seams");
    log.open();
}

Status IcdLibrary::Seams::initialize()
{
    AV_SCOPE("IcdLibrary::Seams::initialize");
    return kmt.initialize();
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
