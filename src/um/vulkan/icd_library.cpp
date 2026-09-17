#include "um/vulkan/icd_library.hpp"

#include "core/trace/function_scope.hpp"
#include "icd/vulkan/icd_binding.hpp"

namespace av {

IcdLibrary::Seams::Seams()
    : heap(), lock(), send_lock(), memory_lock(), log(L"rvkvk"), kmt(), windows(),
      root(heap, kmt, lock, send_lock, memory_lock, log, icd::icd_binding, windows), trace()
{
    AV_SCOPE("IcdLibrary::Seams::Seams");
    log.open();
}

icd::IcdRoot *IcdLibrary::root()
{
    AV_SCOPE("IcdLibrary::root");
    if (!seams_.constructed() && !refused_ && seams_.construct() == Status::ok &&
        seams_.object().kmt.initialize() != Status::ok) {
        seams_.object().log.write("rvkvk: no D3DKMT wait event, refusing the loader");
        seams_.destroy();
        refused_ = true;
        return nullptr;
    }
    if (seams_.constructed() && !started_trace_) {
        started_trace_ = true;
        Seams &seams = seams_.object();
        const Status traced = seams.trace.start("rvkvk", seams.root, crashed_);
        if (traced != Status::ok) seams.log.write("rvkvk: tracing is off: its section, pin or service failed");
    }
    return seams_.constructed() ? &seams_.object().root : nullptr;
}

VkResult IcdLibrary::negotiate(uint32_t *version)
{
    AV_SCOPE("IcdLibrary::negotiate");
    icd::IcdRoot *const answering = root();
    return answering != nullptr ? answering->negotiate(version) : VK_ERROR_INCOMPATIBLE_DRIVER;
}

PFN_vkVoidFunction IcdLibrary::instance_proc_addr(VkInstance instance, const char *name)
{
    AV_SCOPE("IcdLibrary::instance_proc_addr");
    icd::IcdRoot *const answering = root();
    return answering != nullptr ? answering->instance_proc_addr(instance, name) : nullptr;
}

long IcdLibrary::crashed(void *pointers)
{
    AV_SCOPE("IcdLibrary::crashed");
    return seams_.constructed() ? seams_.object().trace.crashed(pointers) : 0;
}

}  // namespace av
