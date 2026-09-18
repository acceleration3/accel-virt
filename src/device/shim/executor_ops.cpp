/* The one symbol this library exports, and the table it answers.
 *
 * In the shim because it is the process's own wiring: it builds the loaders
 * the host really has and hands them to the root, and nothing but a dlopen of
 * the finished library ever reaches it. The conformance suite is what holds it
 * to the ABI; a unit test linking the core has no way to be the process that
 * loaded it.
 *
 * An ABI boundary the constitution permits and the only place in the library
 * where a function is not a method: `rvk_executor_ops()` takes no argument, so
 * there is nowhere to put a `this`, and the root lives in storage of its own
 * for exactly that reason (CLAUDE.md). No logic lives here - each entry point
 * is one line into ExecutorRoot.
 */
#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/context_session.hpp"
#include "device/executor/executor_root.hpp"
#include "device/shim/host_trace.hpp"
#include "device/shim/loader_host_apis.hpp"
#include "device/shim/reporter_state_source.hpp"
#include "device/shim/state_socket.hpp"

#include <dlfcn.h>

namespace {

/* Built on the first question and never destroyed: QEMU closes the module and
 * the process ends, and a destructor here would run after the device has gone. */
class Library {
public:
    /* A session, or null: the device turns a null into a context fault and
     * keeps every other context running.
     *
     * A sub-API with no backend is refused by the root rather than given a
     * session that would answer nothing; the capset already says which those
     * are, because it offers only what a GPU here reports running. */
    av::ContextSession *create(uint32_t sub_api, const uint8_t *gpu_uuid, const RvkContextOptions *options,
                               av::ContextSession *share)
    {
        AV_SCOPE("Library::create");
        return root().open(sub_api, gpu_uuid, options, share);
    }

    void destroy(av::ContextSession *session)
    {
        AV_SCOPE("Library::destroy");
        root().close(session);
    }

    av::ExecutorRoot &root()
    {
        AV_SCOPE("Library::root");
        if (!asked_) {
            asked_ = true;
            /* The library's own table, before anything it runs opens a scope
             * worth reading; a host without /dev/shm runs untraced. */
            (void)trace_.construct();
            (void)trace_.object().start("executor");
            (void)apis_.construct();
            (void)root_.construct();
            apis_.object().open();
            /* A host that serves nothing is a host this still answers for: the
             * root then offers no sub-API and the device describes no capset,
             * which is the honest outcome rather than a failed realize. */
            (void)root_.object().initialize(apis_.object());
            if (trace_.object().started()) serve_state();
        }
        return root_.object();
    }

private:
    /* The root's dump on /dev/shm/rvk-state-executor-<pid>.sock. The library
     * pins itself first: the socket's thread and each traced thread's signal
     * stack run or free code in this object for the rest of the process, and
     * QEMU closes an executor it no longer needs. */
    void serve_state()
    {
        AV_SCOPE("Library::serve_state");
        Dl_info self = {};
        if (dladdr(static_cast<void *>(this), &self) == 0 || self.dli_fname == nullptr) return;
        if (dlopen(self.dli_fname, RTLD_NOW | RTLD_NOLOAD | RTLD_NODELETE) == nullptr) return;
        (void)source_.construct(root_.object());
        (void)socket_.construct();
        (void)socket_.object().start("executor", trace_.object().process(), source_.object());
    }

    av::ObjectStorage<av::HostTrace>      trace_;
    av::ObjectStorage<av::LoaderHostApis> apis_;
    av::ObjectStorage<av::ExecutorRoot>   root_;
    av::ObjectStorage<av::ReporterStateSource> source_;
    av::ObjectStorage<av::StateSocket>    socket_;
    bool                                  asked_ = false;
};

Library library;

/* The device holds a session as an opaque RvkExecutor *; these two are the
 * only places that spelling is crossed. */
av::ContextSession *session_of(RvkExecutor *executor)
{
    AV_SCOPE("session_of");
    return static_cast<av::ContextSession *>(static_cast<void *>(executor));
}

RvkExecutor *handle_of(av::ContextSession *session)
{
    AV_SCOPE("handle_of");
    return static_cast<RvkExecutor *>(static_cast<void *>(session));
}

uint32_t sub_apis(void *)
{
    AV_SCOPE("sub_apis");
    return library.root().sub_apis();
}

RvkExecutor *create(void *, uint32_t sub_api, const uint8_t *gpu_uuid)
{
    AV_SCOPE("create");
    return handle_of(library.create(sub_api, gpu_uuid, nullptr, nullptr));
}

RvkExecutor *create_with_options(void *, uint32_t sub_api, const uint8_t *gpu_uuid, const RvkContextOptions *options,
                                 RvkExecutor *share)
{
    AV_SCOPE("create_with_options");
    return handle_of(library.create(sub_api, gpu_uuid, options, session_of(share)));
}

void destroy(void *, RvkExecutor *executor)
{
    AV_SCOPE("destroy");
    library.destroy(session_of(executor));
}

uint32_t execute(void *, RvkExecutor *executor, uint32_t command_id, const uint8_t *payload,
                 size_t size, uint8_t *reply, size_t reply_capacity, size_t *reply_size)
{
    AV_SCOPE("execute");
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
    AV_SCOPE("import_kinds");
    return library.root().import_kinds(sub_api, gpu_uuid);
}

uint32_t import_blob(void *, RvkExecutor *executor, uint64_t blob_id,
                     const RvkBlobHandle *handle)
{
    AV_SCOPE("import_blob");
    av::ContextSession *const session = session_of(executor);
    if (session == nullptr || handle == nullptr) return RVK_UNDECODABLE;
    return session->import_blob(blob_id, *handle);
}

void release_blob(void *, RvkExecutor *executor, uint64_t blob_id)
{
    AV_SCOPE("release_blob");
    av::ContextSession *const session = session_of(executor);
    if (session != nullptr) session->release_blob(blob_id);
}

uint64_t imported_handle(void *, RvkExecutor *executor, uint64_t blob_id)
{
    AV_SCOPE("imported_handle");
    return library.root().imported_handle(session_of(executor), blob_id);
}

void set_signal(void *, RvkExecutor *executor, void (*signal)(void *context), void *context)
{
    AV_SCOPE("set_signal");
    library.root().set_signal(session_of(executor), signal, context);
}

const RvkExecutorOps table = {RVK_EXECUTOR_ABI_VERSION,
                              nullptr,
                              sub_apis,
                              create,
                              destroy,
                              execute,
                              import_kinds,
                              import_blob,
                              release_blob,
                              imported_handle,
                              create_with_options,
                              set_signal};

}  // namespace

extern "C" __attribute__((visibility("default"))) const RvkExecutorOps *rvk_executor_ops(void)
{
    return &table;
}
