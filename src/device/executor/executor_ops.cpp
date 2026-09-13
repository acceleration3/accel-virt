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
#include "device/executor/context_session.hpp"
#include "device/executor/executor_root.hpp"
#include "device/executor/vulkan_backend.hpp"
#include "device/shim/loader_host_apis.hpp"

namespace {

/* Built on the first question and never destroyed: QEMU closes the module and
 * the process ends, and a destructor here would run after the device has gone. */
class Library {
public:
    /* A session, or null: the device turns a null into a context fault and
     * keeps every other context running.
     *
     * Only Vulkan today. An OpenGL or OpenCL context is refused rather than
     * given a session that would answer nothing, and the capset already says
     * so, because the root offers only what a GPU here can actually run. */
    av::ContextSession *create(uint32_t sub_api, const uint8_t *gpu_uuid)
    {
        if (sub_api != RVK_EXECUTOR_SUB_API_VULKAN) return nullptr;
        return root().open(sub_api, gpu_uuid, vulkan());
    }

    void destroy(av::ContextSession *session) { root().close(session); }

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
    /* One backend per host API, shared by every context on it: a backend
     * outlives a session as the GPU outlives the context. */
    av::VulkanBackend &vulkan()
    {
        if (!vulkan_.constructed()) (void)vulkan_.construct();
        return vulkan_.object();
    }

    av::ObjectStorage<av::LoaderHostApis> apis_;
    av::ObjectStorage<av::ExecutorRoot>   root_;
    av::ObjectStorage<av::VulkanBackend>  vulkan_;
    bool                                  asked_ = false;
};

Library library;

/* The device holds a session as an opaque RvkExecutor *; these two are the
 * only places that spelling is crossed. */
av::ContextSession *session_of(RvkExecutor *executor)
{
    return static_cast<av::ContextSession *>(static_cast<void *>(executor));
}

RvkExecutor *handle_of(av::ContextSession *session)
{
    return static_cast<RvkExecutor *>(static_cast<void *>(session));
}

uint32_t sub_apis(void *) { return library.root().sub_apis(); }

RvkExecutor *create(void *, uint32_t sub_api, const uint8_t *gpu_uuid)
{
    return handle_of(library.create(sub_api, gpu_uuid));
}

void destroy(void *, RvkExecutor *executor) { library.destroy(session_of(executor)); }

uint32_t execute(void *, RvkExecutor *executor, uint32_t command_id, const uint8_t *payload,
                 size_t size, uint8_t *reply, size_t reply_capacity, size_t *reply_size)
{
    av::ContextSession *const session = session_of(executor);
    size_t                    answer = 0u;
    const uint32_t result = session != nullptr
                                ? session->execute(command_id, payload, size, reply, reply_capacity,
                                                   answer)
                                : RVK_DEVICE_LOST;
    if (reply_size != nullptr) *reply_size = answer;
    return result;
}

uint32_t import_kinds(void *, uint32_t sub_api, const uint8_t *gpu_uuid)
{
    return library.root().import_kinds(sub_api, gpu_uuid);
}

uint32_t import_blob(void *, RvkExecutor *executor, uint64_t blob_id,
                     const RvkBlobHandle *handle)
{
    av::ContextSession *const session = session_of(executor);
    if (session == nullptr || handle == nullptr) return RVK_UNDECODABLE;
    return session->import_blob(blob_id, *handle);
}

void release_blob(void *, RvkExecutor *executor, uint64_t blob_id)
{
    av::ContextSession *const session = session_of(executor);
    if (session != nullptr) session->release_blob(blob_id);
}

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
