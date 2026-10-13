/* The one symbol this library exports, and the table it answers.
 *
 * In the shim because it is the process's own wiring: it builds the loaders
 * the host really has and hands them to the root, and nothing but a dlopen of
 * the finished library ever reaches it. The conformance suite is what holds it
 * to the ABI; a unit test linking the core has no way to be the process that
 * loaded it.
 *
 * An ABI boundary the constitution permits and the only place in the library
 * where a function is not a method: `av_executor_ops()` takes no argument, so
 * there is nowhere to put a `this`, and the root lives in storage of its own
 * for exactly that reason (CLAUDE.md). No logic lives here - each entry point
 * is one line into ExecutorRoot.
 */
#include "core/support/object_storage.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/context_session.hpp"
#include "executor/d3d/frame_target.hpp"
#include "executor/executor_root.hpp"
#include "device/shim/host_trace.hpp"
#include "executor/shim/loader_host_apis.hpp"
#include "executor/shim/reporter_state_source.hpp"
#include "device/shim/state_socket.hpp"
#include "proto/resources/surface_point.hpp"

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
    av::ContextSession *create(uint32_t sub_api, const AvContextOptions &options, av::ContextSession *share)
    {
        AV_SCOPE("Library::create");
        return root().open(sub_api, options, share);
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
    /* The root's dump on /dev/shm/av-state-executor-<pid>.sock. The library
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

/* The device holds a session as an opaque AvExecutor *; these two are the
 * only places that spelling is crossed. */
av::ContextSession *session_of(AvExecutor *executor)
{
    AV_SCOPE("session_of");
    return static_cast<av::ContextSession *>(static_cast<void *>(executor));
}

AvExecutor *handle_of(av::ContextSession *session)
{
    AV_SCOPE("handle_of");
    return static_cast<AvExecutor *>(static_cast<void *>(session));
}

uint32_t sub_apis(void *)
{
    AV_SCOPE("sub_apis");
    return library.root().sub_apis();
}

uint32_t open_domain(void *, const uint8_t *gpu_uuid)
{
    AV_SCOPE("open_domain");
    return library.root().open_domain(gpu_uuid);
}

void close_domain(void *, uint32_t domain)
{
    AV_SCOPE("close_domain");
    library.root().close_domain(domain);
}

AvExecutor *create(void *, uint32_t sub_api, const AvContextOptions *options, AvExecutor *share)
{
    AV_SCOPE("create");
    return options != nullptr ? handle_of(library.create(sub_api, *options, session_of(share))) : nullptr;
}

void destroy(void *, AvExecutor *executor)
{
    AV_SCOPE("destroy");
    library.destroy(session_of(executor));
}

uint32_t execute(void *, AvExecutor *executor, uint32_t command_id, const uint8_t *payload,
                 size_t size, uint8_t *reply, size_t reply_capacity, size_t *reply_size)
{
    AV_SCOPE("execute");
    av::ContextSession *const session = session_of(executor);
    size_t                    answer = 0u;
    const uint32_t result = session != nullptr
                                ? session->execute(command_id, payload, size, reply, reply_capacity,
                                                   answer)
                                : AV_DEVICE_LOST;
    if (reply_size != nullptr) *reply_size = answer;
    return result;
}

uint32_t import_kinds(void *, uint32_t sub_api, const uint8_t *gpu_uuid)
{
    AV_SCOPE("import_kinds");
    return library.root().import_kinds(sub_api, gpu_uuid);
}

uint32_t import_blob(void *, AvExecutor *executor, uint64_t blob_id,
                     const AvBlobHandle *handle)
{
    AV_SCOPE("import_blob");
    av::ContextSession *const session = session_of(executor);
    if (session == nullptr || handle == nullptr) return AV_UNDECODABLE;
    return session->import_blob(blob_id, *handle);
}

void release_blob(void *, AvExecutor *executor, uint64_t blob_id)
{
    AV_SCOPE("release_blob");
    av::ContextSession *const session = session_of(executor);
    if (session != nullptr) session->release_blob(blob_id);
}

uint64_t imported_handle(void *, AvExecutor *executor, uint64_t blob_id)
{
    AV_SCOPE("imported_handle");
    return library.root().imported_handle(session_of(executor), blob_id);
}

void set_signal(void *, AvExecutor *executor, void (*signal)(void *context), void *context)
{
    AV_SCOPE("set_signal");
    library.root().set_signal(session_of(executor), signal, context);
}

uint32_t share_object(void *, AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                      uint64_t wire_handle)
{
    AV_SCOPE("share_object");
    return library.root().share_object(session_of(executor), object_id, kind, size, wire_handle);
}

uint32_t open_object(void *, AvExecutor *executor, uint64_t object_id, uint32_t kind, uint64_t size,
                     uint64_t *wire_handle)
{
    AV_SCOPE("open_object");
    return wire_handle != nullptr ? library.root().open_object(session_of(executor), object_id, kind, size, *wire_handle)
                                  : AV_SHARED_REFUSED;
}

void close_object(void *, AvExecutor *executor, uint64_t object_id)
{
    AV_SCOPE("close_object");
    library.root().close_object(session_of(executor), object_id);
}

uint32_t read_image(void *, uint32_t domain, uint64_t key, void *pixels, uint32_t stride, uint32_t width,
                    uint32_t height)
{
    AV_SCOPE("read_image");
    return library.root().read_image(domain, key, pixels, stride, width, height) ? AV_EXECUTED : AV_UNKNOWN;
}

uint32_t export_image(void *, uint32_t domain, uint64_t key, AvExportedImage *image)
{
    AV_SCOPE("export_image");
    return image != nullptr && library.root().export_image(domain, key, *image) ? AV_EXECUTED : AV_UNKNOWN;
}

uint32_t compose_planes(void *, uint32_t domain, const AvScanoutPlane *planes, uint32_t count, uint32_t width,
                        uint32_t height, AvExportedImage *image)
{
    AV_SCOPE("compose_planes");
    return image != nullptr && library.root().compose_planes(domain, planes, count, width, height, *image)
               ? AV_EXECUTED
               : AV_UNKNOWN;
}

uint32_t map_frame(void *, uint32_t domain, uint64_t surface, uint32_t point, uint32_t wait_ms, AvFrameView *view)
{
    AV_SCOPE("map_frame");
    return view != nullptr && library.root().map_frame(domain, av::SurfacePoint(surface, point), wait_ms, *view)
               ? AV_EXECUTED
               : AV_UNKNOWN;
}

uint32_t write_frame(void *, uint32_t domain, uint64_t surface, uint32_t point, uint32_t wait_ms, int fd,
                     uint64_t bytes, uint32_t pitch, const AvFrameRegion *regions, uint32_t count)
{
    AV_SCOPE("write_frame");
    const av::FrameTarget target(fd, bytes, pitch, regions, count);
    return library.root().write_frame(domain, av::SurfacePoint(surface, point), wait_ms, target) ? AV_EXECUTED
                                                                                                : AV_UNKNOWN;
}

void unmap_frame(void *, uint32_t domain, uint64_t surface, uint32_t point)
{
    AV_SCOPE("unmap_frame");
    library.root().unmap_frame(domain, av::SurfacePoint(surface, point));
}

uint32_t export_surface(void *, uint32_t domain, uint64_t surface, AvExportedImage *image)
{
    AV_SCOPE("export_surface");
    return image != nullptr && library.root().export_surface(domain, surface, *image) ? AV_EXECUTED : AV_UNKNOWN;
}

uint32_t sync_surface(void *, uint32_t domain, uint64_t surface, uint64_t *writes, int *fd)
{
    AV_SCOPE("sync_surface");
    return writes != nullptr && fd != nullptr && library.root().sync_surface(domain, surface, *writes, *fd)
               ? AV_EXECUTED
               : AV_UNKNOWN;
}

void release_shared(void *, uint32_t domain, uint64_t object_id)
{
    AV_SCOPE("release_shared");
    library.root().release_shared(domain, object_id);
}

void end_batch(void *, AvExecutor *executor)
{
    AV_SCOPE("end_batch");
    av::ContextSession *const session = session_of(executor);
    if (session != nullptr) session->end_batch();
}

uint32_t poll(void *, AvExecutor *executor)
{
    AV_SCOPE("poll");
    av::ContextSession *const session = session_of(executor);
    return session != nullptr && session->poll() ? 1u : 0u;
}

void gpu_progress(void *, AvExecutor *executor, uint64_t *marked, uint64_t *reached)
{
    AV_SCOPE("gpu_progress");
    uint64_t                  at = 0u;
    uint64_t                  done = 0u;
    av::ContextSession *const session = session_of(executor);
    if (session != nullptr) session->gpu_progress(at, done);
    if (marked != nullptr) *marked = at;
    if (reached != nullptr) *reached = done;
}

const AvExecutorOps table = {AV_EXECUTOR_ABI_VERSION,
                              nullptr,
                              sub_apis,
                              import_kinds,
                              open_domain,
                              close_domain,
                              create,
                              destroy,
                              execute,
                              end_batch,
                              poll,
                              gpu_progress,
                              set_signal,
                              import_blob,
                              release_blob,
                              imported_handle,
                              share_object,
                              open_object,
                              close_object,
                              release_shared,
                              read_image,
                              export_image,
                              map_frame,
                              unmap_frame,
                              export_surface,
                              sync_surface,
                              compose_planes,
                              write_frame};

}  // namespace

extern "C" __attribute__((visibility("default"))) const AvExecutorOps *av_executor_ops(void)
{
    return &table;
}
