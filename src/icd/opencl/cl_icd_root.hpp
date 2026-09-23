/* rvkcl.dll's decisions: what every entry point the Khronos loader reaches
 * answers. One per process - in the exports' storage in the DLL, the one under
 * test in a test - bound to ClIcdBinding for its life.
 *
 * Nothing opens an adapter until the application asks for a device. The loader
 * enumerates platforms and asks two queries about each in every process that
 * ever calls clGetPlatformIDs, including processes with no interest in this
 * adapter, so those are answered from the platform alone.
 *
 * A cl_context is where a transport lives, so everything below one - a queue,
 * a buffer, a program, a kernel, an event - is an object of that context and
 * dies with it. What the application holds is a ClObject, whose first word is
 * the dispatch table; what crosses the wire is the number the host answered.
 *
 * What is not here is deliberate. Only the commands this feature implements
 * and tests have entries; every other slot of the dispatch table carries the
 * generated refusal, which reports itself once and answers the API's own
 * "this driver does not carry it" (constitution VI). */
#ifndef AV_CLICD_CL_ICD_ROOT_HPP
#define AV_CLICD_CL_ICD_ROOT_HPP

#include "core/support/object_pool.hpp"
#include "core/trace/state_reporter.hpp"
#include "icd/common/icd_connection.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_blocking_wait.hpp"
#include "icd/opencl/cl_device_report.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_event_owner.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/cl_extension_policy.hpp"
#include "icd/opencl/cl_object_table.hpp"
#include "icd/opencl/cl_pending_reads.hpp"
#include "icd/opencl/cl_platform.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>
#include <CL/cl_icd.h>

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {
class LogSink;
class ServiceThread;
class ThreadLock;
}  // namespace av::icd

namespace av::clicd {

class ClIcdBinding;
struct ClIcdRootProbe;

class ClIcdRoot final : public StateReporter, public ClEventOwner {
public:
    /* Events one clWaitForEvents may name. A wait allocates nothing, so the
     * handles live on its stack and this is what bounds them; an application
     * asking for more is told, which is a refusal it can see. */
    static constexpr cl_uint max_waited = 64u;
    /* Room for the host's CL_DEVICE_EXTENSIONS while it is intersected with
     * the policy. It lives on the asking thread's stack, so it is bounded
     * here rather than allocated; the longest list any host driver answers is
     * a few hundred bytes, and a host whose list will not fit is a host this
     * cannot confirm a single extension against, which is its own answer. */
    static constexpr size_t  host_extensions_bytes = 4096u;

    ClIcdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, icd::ThreadLock &send_lock,
              icd::LogSink &log, icd::ServiceThread &watcher_thread, ClIcdBinding &binding);
    ~ClIcdRoot();
    ClIcdRoot(const ClIcdRoot &) = delete;
    ClIcdRoot &operator=(const ClIcdRoot &) = delete;

    /* The table every object this ICD hands out carries in its first word, and
     * the one the loader dispatches through. */
    const cl_icd_dispatch &dispatch() const;

    /* ---- what the loader resolves and calls before anything else --------- */

    /* clIcdGetPlatformIDsKHR. Opens nothing. */
    cl_int platform_ids(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms);
    /* clGetExtensionFunctionAddress: the one export, and the only name it
     * answers is the enumeration above. */
    void  *extension_function(const char *name);
    cl_int platform_info(cl_platform_id platform, cl_platform_info name, size_t value_bytes,
                         void *value, size_t *size_ret);

    /* ---- devices -------------------------------------------------------- */

    /* One device per rvkgpu adapter, made on the first ask and kept. */
    cl_int device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                      cl_device_id *devices, cl_uint *num_devices);
    cl_int device_info(cl_device_id device, cl_device_info name, size_t value_bytes, void *value,
                       size_t *size_ret);

    /* ---- contexts ------------------------------------------------------- */

    /* `properties` is the application's zero-terminated list, which the API
     * requires this to judge rather than ignore: an unknown property is
     * CL_INVALID_PROPERTY, and a driver that quietly dropped one would be
     * doing something other than what it was asked. */
    cl_context create_context(const uint64_t *properties, const cl_device_id *devices,
                              cl_uint num_devices, cl_int *error);
    cl_int     retain_context(cl_context context);
    cl_int     release_context(cl_context context);

    /* ---- everything an object of a context is ---------------------------- */

    cl_command_queue create_queue(cl_context context, cl_device_id device,
                                  const uint64_t *properties, cl_int *error);
    cl_mem           create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                   void *host_pointer, const uint64_t *properties, cl_int *error);
    cl_program       create_program(cl_context context, cl_uint count, const char **sources,
                                    const size_t *lengths, cl_int *error);
    cl_kernel        create_kernel(cl_program program, const char *name, cl_int *error);
    cl_int           build_program(cl_program program, const char *options);
    cl_int           set_kernel_argument(cl_kernel kernel, cl_uint index, size_t bytes,
                                         const void *value);
    cl_int           enqueue_kernel(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                    const size_t *offset, const size_t *global, const size_t *local,
                                    cl_event *event);
    cl_int           enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                  size_t offset, size_t bytes, void *into, cl_event *event);
    cl_int           enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                   size_t offset, size_t bytes, const void *from, cl_event *event);
    cl_int           finish(cl_command_queue queue);
    cl_int           flush(cl_command_queue queue);
    /* `cl_khr_external_memory`'s acquire and release, which are the same call
     * here: the object was acquired on the host when the device opened it and
     * stays acquired for its life, so what is left of their meaning is the
     * argument checking and the event. Every named object must be one this
     * context imported and must belong to the queue's context; the event, when
     * one is asked for, is the queue's own place as a marker, as every other
     * enqueue of this ICD answers one (feature 053, research.md §21). */
    cl_int           enqueue_external(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                                      cl_event *event);

    /* ---- maps and shared virtual memory (US2) --------------------------- */

    /* A pointer the application may read and write, staged in guest memory
     * and filled from the buffer; null with the API's own error. */
    void  *map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking, cl_map_flags flags,
                      size_t offset, size_t bytes, cl_event *event, cl_int *error);
    /* Writes a staged mapping back where the application asked to write, then
     * gives it up. CL_INVALID_VALUE for a pointer no live mapping answered. */
    cl_int unmap(cl_command_queue queue, cl_mem buffer, void *pointer, cl_event *event);

    /* Coarse-grained shared virtual memory: the application is handed a
     * mirror, and the host address it stands for is what crosses the wire. */
    void  *svm_alloc(cl_context context, cl_svm_mem_flags flags, size_t bytes, cl_uint alignment);
    void   svm_free(cl_context context, void *pointer);
    /* Fills the mirror from the host; an unmap writes it back. Host access to
     * a coarse-grained allocation is defined only between the two. */
    cl_int svm_map(cl_command_queue queue, cl_bool blocking, cl_map_flags flags, void *pointer,
                   size_t bytes, cl_event *event);
    cl_int svm_unmap(cl_command_queue queue, void *pointer, cl_event *event);
    /* A kernel argument that is an address rather than an object: translated
     * before it is sent, and refused here when it is inside no allocation. */
    cl_int set_kernel_svm_argument(cl_kernel kernel, cl_uint index, const void *pointer);

    /* ---- events (US3) ---------------------------------------------------- */

    /* An event the application can complete itself, which is what a
     * dependency between its own work and the device's is expressed with. */
    cl_event create_user_event(cl_context context, cl_int *error);
    cl_int   set_user_event_status(cl_event event, cl_int status);
    /* Sleeps on the transport until the host has reached them; it does not
     * poll, because a synchronous command's reply is what the wait is. */
    cl_int   wait_for_events(cl_uint count, const cl_event *events);
    cl_int   event_info(cl_event event, cl_event_info name, size_t value_bytes, void *value,
                        size_t *size_ret);
    /* Registers a callback on the watcher's thread. No guest function pointer
     * crosses the wire: the device registers its own with the host, and its
     * completion wakes this context's worker. */
    cl_int   watch_event(cl_event event, cl_int status,
                         ClWatchedEvent::Notify notify, void *user_data);
    /* The watcher, before it runs a callback: a callback on a non-blocking
     * read has to see the bytes. Reached only from the watcher's thread. */
    void     deliver_watched(ClObject &event) override;
    /* The watcher giving back what watch_event took. Reached only from the
     * watcher's thread, after a callback has run. */
    void     release_watched(ClObject &event) override;

    /* Retain and release for every kind the ICD carries: the application's
     * count is the ICD's, and the host is told once the last one goes. */
    cl_int retain(void *object, ClObject::Kind kind);
    cl_int release(void *object, ClObject::Kind kind);

    /* ---- what a state dump says ----------------------------------------- */

    void report(StateWriter &writer) override;
    bool invariants_hold() const;

private:
    friend struct ClIcdRootProbe;

    /* The context an object belongs to, and its transport, or null - which is
     * what every entry asks first. */
    GuestClContext  *context_of(ClObject &object);
    ClientTransport *wire_of(ClObject &object);
    /* Judges an application's pointer and answers the object, or null. */
    ClObject        *judged(const void *pointer, ClObject::Kind kind);
    /* CL_DEVICE_EXTENSIONS, which is neither the host's answer nor this ICD's
     * but the two intersected: the host says the GPU has an extension and a
     * policy row says this ICD carries it, and an application is shown only
     * what both say. */
    /* Whether `name` is exactly `wanted`, which is how the loader's by-name
     * asks are answered without a string library. */
    bool             spells(const char *name, const char *wanted) const;
    /* CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR, answered from what
     * this driver takes rather than from the host: the host's own types are
     * file descriptors and a guest has none. */
    cl_int           import_handle_types(ClObject &device, size_t value_bytes, void *value,
                                         size_t *size_ret);
    cl_int           device_extensions(ClObject &device, size_t value_bytes, void *value,
                                       size_t *size_ret);
    /* CL_SUCCESS for a list this ICD can honour: terminated inside the bound,
     * no property named twice, and every name one of `allowed`. */
    cl_int           judge(const uint64_t *properties, const uint64_t *allowed,
                           uint32_t allowed_count, ClPropertyList &read) const;
    /* Fetches the bytes of every non-blocking read waiting on one of these
     * events, now that the host has reached them. The first failure, or
     * CL_SUCCESS - including for events nothing was waiting on. */
    cl_int           deliver_reads(ClientTransport &wire, const uint64_t *events, uint32_t count);
    /* Every read outstanding on one queue, for a finish - which says
     * everything queued on it has run. */
    cl_int           deliver_queued(ClientTransport &wire, uint64_t queue);
    /* Opens the device this ICD shows, once, on the first ask. */
    [[nodiscard]] Status open_device();
    /* Gives one host object back, by the kind that says which call does it.
     * The one place that knows the mapping, so a guest object that cannot be
     * made and one the application released take the same path. A context is
     * not here: its own teardown takes everything on it. */
    void             give_back_to_host(ClientTransport &wire, ClObject::Kind kind, uint64_t handle);
    /* Makes one object of a host answer, releasing the host's if this cannot
     * hold it - a number the application could never use is worse than an
     * error it can see. */
    /* `transport` is the one the caller has just used, because an object the
     * guest cannot keep has to be given back over it - and every caller has
     * one in hand by the time it gets here. */
    void            *wrap(ClientTransport &transport, ClObject::Kind kind, uint64_t wire,
                          GuestClContext *context,
                          ClObject *parent, cl_int *error);
    /* What an entry answers when the pointer it was given is not an object of
     * that kind: the API's own error for each. */
    cl_int           refusal_for(ClObject::Kind kind) const;

    icd::ThreadLock       &lock_;
    icd::LogSink          &log_;
    ClIcdBinding          &binding_;
    ClDispatchTable        dispatch_;
    ClPlatform             platform_;
    ClExtensionPolicy      extensions_;
    ClObjectTable          objects_;
    icd::IcdConnection     connection_;
    /* The NT handles this process holds for shared objects, which every
     * context's imports resolve through (feature 053). */
    icd::SharedHandles     shared_handles_;
    ObjectPool<GuestClContext> contexts_;
    BlockSource           &memory_;
    icd::ThreadLock       &send_lock_;
    /* The one device this ICD shows, made on the first clGetDeviceIDs. */
    ClDeviceReport         device_;
    ClObject              *device_object_;
    ClEventWatcher         watcher_;
    ClBlockingWait         waits_;
    ClPendingReads         reads_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ICD_ROOT_HPP
