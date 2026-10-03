/* avcl.dll's decisions: what every entry point the Khronos loader reaches
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
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_object_table.hpp"
#include "icd/opencl/cl_pending_reads.hpp"
#include "icd/opencl/cl_refusal_log.hpp"
#include "icd/opencl/cl_platform.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_icd.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ClientTransport;
class Kmt;
class StateWriter;
}  // namespace av

namespace av::icd {
class ServiceThread;
class ThreadLock;
}  // namespace av::icd

namespace av::clicd {

class ClArenaBlocks;
class ClBufferRect;

class D3dAdapterSource;
class GlShareSource;
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

    /* `objects_lock` is the object table's own (ClObjectTable): every entry
     * point looks its arguments up there without `lock`. `blocks_lock` is
     * every context's arena blocks' (ClArenaBlocks). */
    ClIcdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock, icd::ThreadLock &send_lock,
              icd::ThreadLock &objects_lock, icd::ThreadLock &blocks_lock, LogSink &log, icd::ServiceThread &watcher_thread, GlShareSource &gl_share,
              D3dAdapterSource &d3d_adapters,
              ClIcdBinding &binding);
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

    /* One device per avgpu adapter, made on the first ask and kept. */
    cl_int device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                      cl_device_id *devices, cl_uint *num_devices);
    cl_int device_info(cl_device_id device, cl_device_info name, size_t value_bytes, void *value,
                       size_t *size_ret);

    /* ---- contexts ------------------------------------------------------- */

    /* `properties` is the application's zero-terminated list, which the API
     * requires this to judge rather than ignore: an unknown property is
     * CL_INVALID_PROPERTY, and a driver that quietly dropped one would be
     * doing something other than what it was asked. CL_GL_CONTEXT_KHR (with
     * CL_WGL_HDC_KHR) makes the context from that OpenGL context, which must
     * be current on the calling thread and one this process's OpenGL ICD
     * made; any other is CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR. */
    cl_context create_context(const cl_context_properties *properties, const cl_device_id *devices,
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

    /* ---- what a program and a kernel say about themselves on a device --- */

    /* Asked of the host about its own device, which is the one this ICD's
     * device stands for. */
    cl_int program_build_info(cl_program program, cl_device_id device, cl_program_build_info name,
                              size_t value_bytes, void *value, size_t *size_ret);
    cl_int kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                  cl_kernel_work_group_info name, size_t value_bytes, void *value,
                                  size_t *size_ret);

    /* ---- transfers the device makes on its own ------------------------- */

    /* Nothing crosses but the two handles and the offsets, so these are
     * enqueued as they are and the host's own event answered. */
    cl_int enqueue_copy_buffer(cl_command_queue queue, cl_mem from, cl_mem to, size_t from_offset,
                               size_t to_offset, size_t bytes, cl_event *event);
    /* The pattern is at most 128 bytes (the API's own bound) and crosses with
     * the command; the host copies it before it answers. */
    cl_int enqueue_fill_buffer(cl_command_queue queue, cl_mem buffer, const void *pattern,
                               size_t pattern_bytes, size_t offset, size_t bytes, cl_event *event);
    cl_int enqueue_copy_image(cl_command_queue queue, cl_mem from, cl_mem to,
                              const size_t *from_origin, const size_t *to_origin,
                              const size_t *region, cl_event *event);

    /* ---- images and samplers -------------------------------------------- */

    /* An image of the host's making. A buffer the description names is
     * translated to the host's number for it. Bytes copied from the
     * application's pointer are written after the create, in commands that
     * fit, on a queue of the ICD's own - an image of any size can be made from
     * memory - for the image types whose layout is rows and slices. Memory the
     * application keeps (CL_MEM_USE_HOST_PTR) is refused: nothing here could
     * keep the host's image and the application's pointer the same. */
    cl_mem create_image(cl_context context, cl_mem_flags flags, const cl_image_format *format,
                        const cl_image_desc *desc, void *host_pointer, cl_int *error);
    /* The 1.1 calls, which a program written for 1.1 makes every image with:
     * the same image clCreateImage makes, described as it would be told. */
    cl_mem create_image_2d(cl_context context, cl_mem_flags flags, const cl_image_format *format, size_t width,
                           size_t height, size_t row_pitch, void *host_pointer, cl_int *error);
    cl_mem create_image_3d(cl_context context, cl_mem_flags flags, const cl_image_format *format, size_t width,
                           size_t height, size_t depth, size_t row_pitch, size_t slice_pitch, void *host_pointer,
                           cl_int *error);
    /* Image transfers are always complete when they return: a non-blocking
     * one may be, and the bytes are then valid before the event says so,
     * which the API allows. Both wait for the queue first and then go as
     * commands that fit (ClTransferChunks). */
    cl_int enqueue_read_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                              const size_t *region, size_t row_pitch, size_t slice_pitch,
                              void *into, cl_event *event);
    cl_int enqueue_write_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                               const size_t *region, size_t row_pitch, size_t slice_pitch,
                               const void *from, cl_event *event);
    /* A buffer's rectangle, as an image's region is moved: the queue drained,
     * then commands that fit, so a read is complete when it returns whatever
     * the application asked for - which the API allows. */
    cl_int enqueue_read_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect, void *into,
                                    cl_event *event);
    cl_int enqueue_write_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                     const void *from, cl_event *event);
    /* Nothing crosses but handles and the geometry: enqueued as it is, the
     * host refusing what the API refuses. */
    cl_int enqueue_copy_buffer_rect(cl_command_queue queue, cl_mem from, cl_mem to, const size_t *from_origin,
                                    const size_t *to_origin, const size_t *region, size_t from_row_pitch,
                                    size_t from_slice_pitch, size_t to_row_pitch, size_t to_slice_pitch,
                                    cl_event *event);
    /* The three sampler properties and nothing else; the deprecated
     * clCreateSampler is the same list written out. */
    cl_sampler create_sampler(cl_context context, const uint64_t *properties, cl_int *error);
    /* The host's formats for an image of `type`: a format holds no handle, so
     * the answer is the host's as it stands. */
    cl_int supported_image_formats(cl_context context, cl_mem_flags flags, cl_mem_object_type type,
                                   cl_uint num_entries, cl_image_format *formats, cl_uint *num_formats);

    /* ---- cl_khr_gl_sharing ------------------------------------------------ */

    /* An OpenCL object over an OpenGL one of the context `context` was made
     * from, named as the application names it in GL - the host resolves the
     * name in that context's share group. CL_INVALID_CONTEXT for a context not
     * made from a GL context; the host's own refusal otherwise. Every texture
     * spelling - clCreateFromGLTexture and the 1.1 pair - is one call. */
    cl_mem create_from_gl_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *error);
    cl_mem create_from_gl_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                  cl_GLuint texture, cl_int *error);
    cl_mem create_from_gl_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                       cl_int *error);
    cl_int gl_object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name);
    cl_int gl_texture_info(cl_mem object, cl_gl_texture_info name, size_t value_bytes, void *value,
                           size_t *size_ret);
    /* Acquire and release, which the host runs in the queue's order after the
     * wait list: every object must be one of the queue's context. */
    cl_int enqueue_acquire_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                              const cl_event *events, cl_event *event);
    cl_int enqueue_release_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                              const cl_event *events, cl_event *event);
    /* clGetGLContextInfoKHR, answered here: the one device this ICD shows is
     * the one every OpenGL context of ours runs on. */
    cl_int gl_context_info(const cl_context_properties *properties, cl_gl_context_info name, size_t value_bytes,
                           void *value, size_t *size_ret);

    /* ---- cl_khr_d3d10_sharing --------------------------------------------- */

    /* The extension's own numbers (cl_d3d10.h), which portable code cannot
     * include: it includes d3d10.h. */
    static constexpr cl_uint d3d10_device_source = 0x4010u;
    static constexpr cl_uint d3d10_adapter_source = 0x4011u;
    static constexpr cl_uint d3d10_preferred_devices = 0x4012u;
    static constexpr cl_uint d3d10_all_devices = 0x4013u;
    static constexpr cl_uint d3d10_context_device = 0x4014u;
    static constexpr cl_int  invalid_d3d10_device = -1002;

    /* clGetDeviceIDsFromD3D10KHR: this ICD's one device for a DXGI adapter or
     * an ID3D10Device on the avgpu adapter that device runs on - preferred
     * and all alike, since there is one - and CL_DEVICE_NOT_FOUND for any
     * other adapter, which is how an application tells which of its adapters
     * a device of ours belongs to. */
    cl_int device_ids_from_d3d10(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                 cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
    /* An object over a Direct3D 10 one, and its acquire and release, belong to
     * a context made from a Direct3D 10 device, and no context here is one:
     * create_context answers such a device CL_INVALID_D3D10_DEVICE_KHR, the
     * extension's word for a device this one cannot share with. So a context
     * or queue of this ICD's is answered CL_INVALID_CONTEXT, and anything else
     * the API's own error for it. */
    cl_mem create_from_d3d10(cl_context context, cl_int *error);
    cl_int enqueue_d3d10_objects(cl_command_queue queue);

    /* ---- the queue's own points ------------------------------------------ */

    /* A marker or a barrier behind `count` events of this context, named to the
     * host by its numbers for them. A queue here runs in order, so what either
     * adds is the wait on events of another queue, and the event. */
    cl_int enqueue_marker(cl_command_queue queue, cl_uint count, const cl_event *events,
                          cl_event *event);
    cl_int enqueue_barrier(cl_command_queue queue, cl_uint count, const cl_event *events,
                           cl_event *event);
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
    /* An entry point this ICD does not serve was called, and refused: named
     * in the log, since an application that only reports "a problem" leaves
     * nothing else to say which. */
    void     note_refused(const char *entry_point);
    /* `result`, and in the log the first time an entry point answers it, when
     * it is an error. */
    cl_int   noted(const char *entry_point, cl_int result);
    /* clGetContextInfo, clGetMemObjectInfo and clGetImageInfo: an answer that
     * names an object is this ICD's handle for it; the rest is the host's. */
    cl_int   context_info(cl_context context, cl_context_info name, size_t value_bytes, void *value,
                          size_t *size_ret);
    cl_int   mem_object_info(cl_mem object, cl_mem_info name, size_t value_bytes, void *value,
                             size_t *size_ret);
    cl_int   image_info(cl_mem image, cl_image_info name, size_t value_bytes, void *value, size_t *size_ret);
    /* The host GPU's timestamps for the command, on a queue made with
     * CL_QUEUE_PROFILING_ENABLE. */
    cl_int   event_profiling(cl_event event, cl_profiling_info name, size_t value_bytes, void *value,
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
    template <typename Property>
    cl_int           judge(const Property *properties, const uint64_t *allowed,
                           uint32_t allowed_count, ClPropertyList &read) const;
    /* Fetches the bytes of every non-blocking read waiting on one of these
     * events, now that the host has reached them. The first failure, or
     * CL_SUCCESS - including for events nothing was waiting on. */
    cl_int           deliver_reads(ClientTransport &wire, ClArenaBlocks &blocks, const uint64_t *events,
                                   uint32_t count);
    /* Every read outstanding on one queue, for a finish - which says
     * everything queued on it has run. */
    cl_int           deliver_queued(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t queue);
    /* Waits for everything enqueued on `queue` so far without holding the
     * host's worker: a marker is the queue's end as an event, and this side
     * asks after it (ClBlockingWait). What a finish is, and what every
     * transfer that asks the host to block does first. */
    cl_int           drain(ClientTransport &wire, uint64_t queue);
    /* A drain, unless the queue is known idle; idle after one (ClObject). */
    [[nodiscard]] cl_int drain_unless_idle(ClientTransport &wire, ClObject &queue);
    /* The queue, looked up for a command that leaves work on it: it is no
     * longer known idle. */
    ClObject        *busied(cl_command_queue queue);
    /* The event a transfer that completed inside its call hands back when one
     * was asked for: a marker in the queue, which is complete as soon as the
     * host looks. CL_SUCCESS with nothing made when `event` is null. */
    cl_int           marker_event(ClientTransport &wire, ClObject &queue, cl_event *event);
    /* The host's numbers for an application's wait list, into `into` (room for
     * max_waited): CL_INVALID_EVENT_WAIT_LIST for a list the API refuses or an
     * event of another context, CL_OUT_OF_RESOURCES past max_waited. */
    cl_int           wait_list(GuestClContext &context, cl_uint count, const cl_event *events,
                               uint64_t *into);
    /* The share name of the OpenGL context `list` names, into `share`: 0 for a
     * list naming none, CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR for one that
     * names a context this process's OpenGL ICD cannot vouch for. */
    cl_int           gl_share_for(const ClPropertyList &list, uint32_t &share);
    /* The context object `context` names when it was made from a GL context
     * and can still send; null with the error set otherwise. */
    ClObject        *gl_sharing(cl_context context, cl_int *error);
    void             note_build_log(ClientTransport &wire, uint64_t program, const char *options);
    /* What a host create over a GL object answered, wrapped as a mem object. */
    cl_mem           gl_object(ClientTransport &wire, ClObject &context, uint64_t made, cl_int host_error,
                               cl_int *error);
    using GlObjectsCommand = cl_int (*)(ClientTransport &, uint64_t, cl_uint, const uint64_t *, cl_uint,
                                        const uint64_t *, uint64_t *);
    cl_int           enqueue_gl_objects(GlObjectsCommand command, cl_command_queue queue, cl_uint count,
                                        const cl_mem *objects, cl_uint waits, const cl_event *events,
                                        cl_event *event);
    /* A marker or a barrier: which of the two generated calls sends it is the
     * whole difference. */
    using PointCommand = cl_int (*)(ClientTransport &, uint64_t, cl_uint, const uint64_t *, uint64_t *);
    cl_int           enqueue_point(PointCommand command, cl_command_queue queue, cl_uint count,
                                   const cl_event *events, cl_event *event);
    /* CL_IMAGE_ELEMENT_SIZE, asked of the host; zero when it will not say. */
    size_t           element_bytes(ClientTransport &wire, uint64_t image);
    /* Writes the application's bytes into an image just made, on a queue of
     * its own that is released again; CL_INVALID_OPERATION for an image type
     * whose layout is not rows and slices. */
    cl_int           fill_image(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t image,
                                const cl_image_desc &desc, const void *from);
    /* The same for a buffer whose bytes one create could not carry. */
    cl_int           fill_buffer(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t buffer,
                                 size_t bytes,
                                 const void *from);
    /* A host queue on `context` that nothing else has, for a fill: nothing is
     * ahead of its writes for the host to wait behind. 0, with the error, when
     * the host will not make one. */
    uint64_t         own_queue(ClientTransport &wire, uint64_t context, cl_int &error);
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
    LogSink          &log_;
    GlShareSource         &gl_share_;
    D3dAdapterSource      &d3d_adapters_;
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
    icd::ThreadLock       &blocks_lock_;
    /* The one device this ICD shows, made on the first clGetDeviceIDs. */
    ClDeviceReport         device_;
    ClObject              *device_object_;
    ClEventWatcher         watcher_;
    ClBlockingWait         waits_;
    ClPendingReads         reads_;
    ClRefusalLog           refusals_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ICD_ROOT_HPP
