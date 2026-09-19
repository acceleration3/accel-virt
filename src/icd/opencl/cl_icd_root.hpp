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
#include "icd/opencl/cl_device_report.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_event_owner.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/cl_extension_policy.hpp"
#include "icd/opencl/cl_object_table.hpp"
#include "icd/opencl/cl_platform.hpp"
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

    cl_context create_context(const cl_device_id *devices, cl_uint num_devices, cl_int *error);
    cl_int     retain_context(cl_context context);
    cl_int     release_context(cl_context context);

    /* ---- everything an object of a context is ---------------------------- */

    cl_command_queue create_queue(cl_context context, cl_device_id device, cl_int *error);
    cl_mem           create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                   void *host_pointer, cl_int *error);
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
    /* Opens the device this ICD shows, once, on the first ask. */
    [[nodiscard]] Status open_device();
    /* Makes one object of a host answer, releasing the host's if this cannot
     * hold it - a number the application could never use is worse than an
     * error it can see. */
    void            *wrap(ClObject::Kind kind, uint64_t wire, GuestClContext *context,
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
    ObjectPool<GuestClContext> contexts_;
    BlockSource           &memory_;
    icd::ThreadLock       &send_lock_;
    /* The one device this ICD shows, made on the first clGetDeviceIDs. */
    ClDeviceReport         device_;
    ClObject              *device_object_;
    ClEventWatcher         watcher_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ICD_ROOT_HPP
