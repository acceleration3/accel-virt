#include "um/vulkan/icd_library.hpp"

#include "icd/vulkan/icd_binding.hpp"

namespace av {

IcdLibrary::Seams::Seams()
    : heap(), lock(), send_lock(), memory_lock(), log(), kmt(),
      root(heap, kmt, lock, send_lock, memory_lock, log, icd::icd_binding)
{
    log.open();
}

icd::IcdRoot *IcdLibrary::root()
{
    if (!seams_.constructed() && !refused_ && seams_.construct() == Status::ok &&
        seams_.object().kmt.initialize() != Status::ok) {
        seams_.object().log.write("rvkvk: no D3DKMT wait event, refusing the loader");
        seams_.destroy();
        refused_ = true;
    }
    return seams_.constructed() ? &seams_.object().root : nullptr;
}

VkResult IcdLibrary::negotiate(uint32_t *version)
{
    icd::IcdRoot *const answering = root();
    return answering != nullptr ? answering->negotiate(version) : VK_ERROR_INCOMPATIBLE_DRIVER;
}

PFN_vkVoidFunction IcdLibrary::instance_proc_addr(VkInstance instance, const char *name)
{
    icd::IcdRoot *const answering = root();
    return answering != nullptr ? answering->instance_proc_addr(instance, name) : nullptr;
}

}  // namespace av
