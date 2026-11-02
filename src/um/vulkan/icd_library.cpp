#include "um/vulkan/icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/icd_binding.hpp"

namespace av {

IcdLibrary::Seams::Seams()
    : process(L"avvk", "avvk: "), lock(), send_lock(), memory_lock(), windows(), chains_lock(),
      chains(process.heap(), process.kmt(), chains_lock, process.log()), present_lock(), present_thread(),
      present_handed(), present_shown(),
      root(process.heap(), process.kmt(), lock, send_lock, memory_lock, process.log(), icd::icd_binding, windows, chains,
           present_thread, present_handed, present_shown, present_lock)
{
    AV_SCOPE("IcdLibrary::Seams::Seams");
    if (present_handed.initialize() != Status::ok || present_shown.initialize() != Status::ok) {
        process.log().write("no present thread: its wake events could not be made");
        root.presentation().refuse_presents();
    }
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

}
