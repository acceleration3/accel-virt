/* The one symbol this library exports, and the table it answers.
 *
 * An ABI boundary the constitution permits and the only place in the library
 * where a function is not a method: `rvk_executor_ops()` takes no argument, so
 * there is nowhere to put a `this`, and the root lives in storage of its own
 * for exactly that reason (CLAUDE.md). No logic lives here - each entry point
 * is one line into ExecutorRoot.
 */
#include "core/support/object_storage.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/executor_root.hpp"
#include "device/shim/loader_host_apis.hpp"

namespace {

/* Built on the first question and never destroyed: QEMU closes the module and
 * the process ends, and a destructor here would run after the device has gone. */
class Library {
public:
    av::ExecutorRoot &root()
    {
        if (!asked_) {
            asked_ = true;
            (void)apis_.construct();
            (void)root_.construct();
            apis_.object().open();
            /* A host that serves nothing is a host this still answers for: the
             * root then offers no sub-API and the device describes no capset,
             * which is the honest outcome rather than a failed realize. */
            (void)root_.object().initialize(apis_.object());
        }
        return root_.object();
    }

private:
    av::ObjectStorage<av::LoaderHostApis> apis_;
    av::ObjectStorage<av::ExecutorRoot>   root_;
    bool                                  asked_ = false;
};

Library library;

uint32_t sub_apis(void *) { return library.root().sub_apis(); }

RvkExecutor *create(void *, uint32_t, const uint8_t *) { return nullptr; }

void destroy(void *, RvkExecutor *) {}

uint32_t execute(void *, RvkExecutor *, uint32_t, const uint8_t *, size_t, uint8_t *, size_t,
                 size_t *)
{
    return RVK_DEVICE_LOST;
}

uint32_t import_kinds(void *, uint32_t sub_api, const uint8_t *gpu_uuid)
{
    return library.root().import_kinds(sub_api, gpu_uuid);
}

uint32_t import_blob(void *, RvkExecutor *, uint64_t, const RvkBlobHandle *)
{
    return RVK_UNDECODABLE;
}

void release_blob(void *, RvkExecutor *, uint64_t) {}

const RvkExecutorOps table = {RVK_EXECUTOR_ABI_VERSION,
                              nullptr,
                              sub_apis,
                              create,
                              destroy,
                              execute,
                              import_kinds,
                              import_blob,
                              release_blob};

}  // namespace

extern "C" __attribute__((visibility("default"))) const RvkExecutorOps *rvk_executor_ops(void)
{
    return &table;
}
