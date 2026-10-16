/* avcl.dll's decisions: what every entry point the Khronos loader reaches
 * answers. One per process - in the exports' storage in the DLL, the one under
 * test in a test - bound to ClIcdBinding for its life.
 *
 * Nothing opens an adapter until the application asks for a device. The loader
 * enumerates platforms and asks two queries about each in every process that
 * ever calls clGetPlatformIDs, including processes with no interest in this
 * adapter, so those are answered from the platform alone.
 *
 * One platform, and on it one device per avgpu adapter (ClDevice), in the
 * order AvgpuAdapters gives - the primary display's first, which is the one an
 * application that takes the first device should get. The Khronos loader
 * loads this DLL once however many adapters name it, so the devices of every
 * adapter are this one ICD's to show.
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

#include "core/log/keyed_log.hpp"
#include "core/support/object_pool.hpp"
#include "core/trace/state_reporter.hpp"
#include "icd/common/refused_calls.hpp"
#include "icd/common/shared_handles.hpp"
#include "icd/opencl/cl_blocking_wait.hpp"
#include "icd/opencl/cl_destructor_callbacks.hpp"
#include "icd/opencl/cl_host_numbers.hpp"
#include "icd/opencl/cl_devices.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_event_owner.hpp"
#include "icd/opencl/cl_event_watcher.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_info_answers.hpp"
#include "icd/opencl/cl_object_table.hpp"
#include "icd/opencl/cl_pending_reads.hpp"
#include "icd/opencl/cl_refusal_log.hpp"
#include "icd/opencl/cl_platform.hpp"
#include "icd/opencl/cl_program_notify.hpp"
#include "icd/opencl/cl_property_list.hpp"
#include "icd/opencl/cl_wait_list.hpp"
#include "icd/opencl/d3d11_resource.hpp"
#include "icd/opencl/guest_cl_context.hpp"

#include <CL/cl.h>
#include <CL/cl_gl.h>
#include <CL/cl_icd.h>

namespace av {
class LogSink;
}  // namespace av

namespace av {
class BlockSource;
class ExtensionWords;
class ClientTransport;
class Kmt;
class StateWriter;
class ThreadLock;
}  // namespace av

namespace av::icd {
class ServiceThread;
}  // namespace av::icd

namespace av::clicd {

class ClArenaBlocks;
class ClBufferRect;
class MappedRegion;

class D3d11Bridge;
class D3d11Object;
class D3dAdapterSource;
class GlShareSource;
struct ClIcdRootProbe;

class ClIcdRoot final : public StateReporter, public ClEventOwner {
public:
    /* The least a CL_MEM_USE_HOST_PTR buffer lends the device of the
     * application's memory: smaller ones copy, which costs less than a
     * lending and its host blob, and keeps neighbouring small buffers, which
     * share pages, from refusing each other. */
    static constexpr size_t lent_minimum = 64u * 1024u;
    /* Room for the host's CL_DEVICE_EXTENSIONS while it is intersected with
     * the policy. It lives on the asking thread's stack, so it is bounded
     * here rather than allocated; the longest list any host driver answers is
     * a few hundred bytes, and a host whose list will not fit is a host this
     * cannot confirm a single extension against, which is its own answer. */
    static constexpr size_t  host_extensions_bytes = 4096u;

    /* `objects_lock` is the object table's own (ClObjectTable): every entry
     * point looks its arguments up there without `lock`. `blocks_lock` is
     * every context's arena blocks' (ClArenaBlocks). */
    ClIcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock, ThreadLock &send_lock,
              ThreadLock &objects_lock, ThreadLock &blocks_lock, LogSink &log, icd::ServiceThread &watcher_thread, GlShareSource &gl_share,
              D3dAdapterSource &d3d_adapters, D3d11Bridge &d3d11,
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

    /* Every device, each opened on the first ask and kept; one whose adapter
     * will not open is left out. */
    cl_int device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                      cl_device_id *devices, cl_uint *num_devices);
    cl_int device_info(cl_device_id device, cl_device_info name, size_t value_bytes, void *value,
                       size_t *size_ret);
    /* clRetainDevice and clReleaseDevice: every device here is a root device,
     * which the API keeps no count of, so either is CL_SUCCESS for it. */
    cl_int device_reference(cl_device_id device);
    /* CL_DEVICE_GLOBAL_FREE_MEMORY_AMD (cl_amd_device_attribute_query), which
     * the host's OpenCL does not answer: on an AMD adapter, what Windows says
     * this process may still use of the adapter's own memory, in KiB, as the
     * total and as the largest block - or the total alone, into a room for
     * less than both. */
    static constexpr cl_device_info free_memory_amd = 0x4039u;

    /* ---- contexts ------------------------------------------------------- */

    /* `properties` is the application's zero-terminated list, which the API
     * requires this to judge rather than ignore: an unknown property is
     * CL_INVALID_PROPERTY, and a driver that quietly dropped one would be
     * doing something other than what it was asked. CL_GL_CONTEXT_KHR (with
     * CL_WGL_HDC_KHR) makes the context from that OpenGL context, which must
     * be current on the calling thread and one this process's OpenGL ICD
     * made; any other is CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR. */
    cl_context create_context(const cl_context_properties *properties, const cl_device_id *devices,
                              cl_uint num_devices, cl_int *error,
                              const ClContextNotify &notify = ClContextNotify());
    cl_int     retain_context(cl_context context);
    cl_int     release_context(cl_context context);
    /* clCreateContextFromType: a GPU, the default device or every device is
     * the first device - the primary display's adapter - made into a context
     * as create_context makes one; any other type is no device of this
     * platform's. One context spans one device here (create_context). */
    cl_context create_context_from_type(const cl_context_properties *properties, cl_device_type type,
                                        cl_int *error, const ClContextNotify &notify = ClContextNotify());
    /* clSetContextDestructorCallback and clSetMemObjectDestructorCallback:
     * kept here and run when the object's last holder lets go of it, the
     * latest registered first. */
    cl_int     watch_destruction(cl_context context, ClDestructorCallbacks::ContextCallback callback,
                                 void *user_data);
    cl_int     watch_destruction(cl_mem object, ClDestructorCallbacks::MemCallback callback, void *user_data);
    /* clCreateSubDevices: no device here can be partitioned (ClDeviceReport
     * says so), so every partition asked of one of its devices is the
     * specification's unsupported one. */
    cl_int     create_sub_devices(cl_device_id device, const cl_device_partition_property *properties,
                                  cl_uint num_devices, cl_device_id *out_devices, cl_uint *num_devices_ret);
    /* clGetExtensionFunctionAddressForPlatform: extension_function for this
     * platform, nothing for any other. */
    void      *extension_function_for(cl_platform_id platform, const char *name);

    /* ---- everything an object of a context is ---------------------------- */

    cl_command_queue create_queue(cl_context context, cl_device_id device,
                                  const uint64_t *properties, cl_int *error);
    cl_mem           create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                   void *host_pointer, const uint64_t *properties, cl_int *error);
    /* A CL_MEM_USE_HOST_PTR buffer made over the application's own pages,
     * which the device takes as host memory where they are
     * (docs/host-memory.md, "Application memory"): null, with nothing held,
     * for one under lent_minimum bytes, or that the context, the device or
     * the host would not take - the caller then copies instead. */
    cl_mem           create_over_application(ClObject &context, ClientTransport &wire, cl_mem_flags flags,
                                             size_t bytes, void *host_pointer, cl_int *error);
    cl_program       create_program(cl_context context, cl_uint count, const char **sources,
                                    const size_t *lengths, cl_int *error);
    cl_kernel        create_kernel(cl_program program, const char *name, cl_int *error);
    cl_int           set_kernel_argument(cl_kernel kernel, cl_uint index, size_t bytes,
                                         const void *value);
    cl_int           enqueue_kernel(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                    const size_t *offset, const size_t *global, const size_t *local,
                                    cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int           enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                  size_t offset, size_t bytes, void *into, cl_event *event,
                                  const ClWaitList &waits = ClWaitList());
    cl_int           enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                   size_t offset, size_t bytes, const void *from, cl_event *event,
                                   const ClWaitList &waits = ClWaitList());
    cl_int           finish(cl_command_queue queue);
    cl_int           flush(cl_command_queue queue);

    /* ---- programs (cl_icd_root_programs.cpp) ------------------------------ */

    /* A device list names the context's one device or nothing at all, which
     * the API takes for every device of the context: the same device. */
    cl_int     build_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                             const char *options, const ClProgramNotify &notify = ClProgramNotify());
    /* A binary is what CL_PROGRAM_BINARIES answered: the host's own behind an
     * envelope the host opens (docs/protocol.md, "OpenCL program binaries").
     * One from another GPU or driver, or damaged, is CL_INVALID_BINARY in
     * `binary_status` and in `error`, and the application builds its source
     * again. */
    cl_program create_program_with_binary(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                                          const size_t *lengths, const unsigned char **binaries,
                                          cl_int *binary_status, cl_int *error);
    /* SPIR-V, which the host compiles; a host with no IL answers
     * CL_INVALID_OPERATION as its CL_DEVICE_IL_VERSION says it would. */
    cl_program create_program_with_il(cl_context context, const void *il, size_t length, cl_int *error);
    cl_program create_program_with_built_in_kernels(cl_context context, cl_uint num_devices,
                                                    const cl_device_id *device_list, const char *kernel_names,
                                                    cl_int *error);
    /* Separate compilation: the headers are programs of the same context,
     * named in the program by their include names. */
    cl_int     compile_program(cl_program program, cl_uint num_devices, const cl_device_id *device_list,
                               const char *options, cl_uint num_headers, const cl_program *headers,
                               const char **header_names, const ClProgramNotify &notify);
    /* A new program of `inputs`, which must be programs of `context`. A link
     * that failed still answers the program the host made, whose log says
     * why, as the API has it. */
    cl_program link_program(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                            const char *options, cl_uint num_inputs, const cl_program *inputs,
                            const ClProgramNotify &notify, cl_int *error);
    /* The reference count, the context and the devices are this ICD's;
     * CL_PROGRAM_BINARIES is fetched into the application's own pointers;
     * the rest is the host's answer. */
    cl_int     program_info(cl_program program, cl_program_info name, size_t value_bytes, void *value,
                            size_t *size_ret);
    cl_int     set_specialization_constant(cl_program program, cl_uint id, size_t bytes, const void *value);
    /* No device here runs a program's global destructors (its
     * CL_PROGRAM_SCOPE_GLOBAL_DTORS_PRESENT is the host's, and no host GPU
     * driver has them), which is the API's CL_INVALID_OPERATION once the
     * arguments are judged. */
    cl_int     set_program_release_callback(cl_program program, void(CL_CALLBACK *notify)(cl_program, void *));
    /* A hint the API lets a driver ignore: the host's compiler is the host's. */
    cl_int     unload_platform_compiler(cl_platform_id platform);

    /* ---- kernels (cl_icd_root_programs.cpp) ------------------------------- */

    /* Every kernel of a built program, each an object of this ICD's. */
    cl_int    create_kernels_in_program(cl_program program, cl_uint num_kernels, cl_kernel *kernels,
                                        cl_uint *num_kernels_ret);
    /* A kernel of the same program, with the arguments set so far. */
    cl_kernel clone_kernel(cl_kernel kernel, cl_int *error);
    /* The reference count, the context and the program are this ICD's; the
     * rest is the host's. */
    cl_int    kernel_info(cl_kernel kernel, cl_kernel_info name, size_t value_bytes, void *value, size_t *size_ret);
    cl_int    kernel_arg_info(cl_kernel kernel, cl_uint index, cl_kernel_arg_info name, size_t value_bytes,
                              void *value, size_t *size_ret);
    cl_int    kernel_sub_group_info(cl_kernel kernel, cl_device_id device, cl_kernel_sub_group_info name,
                                    size_t input_bytes, const void *input, size_t value_bytes, void *value,
                                    size_t *size_ret);
    /* The SVM pointers a kernel reaches through other memory, translated as a
     * pointer argument is; fine-grained system SVM, which no device here has,
     * only turned off. */
    cl_int    set_kernel_exec_info(cl_kernel kernel, cl_kernel_exec_info name, size_t bytes, const void *value);

    /* ---- what a program and a kernel say about themselves on a device --- */

    /* Asked of the host about its own device, which is the one the device of
     * the object's context stands for; another device is CL_INVALID_DEVICE. */
    cl_int program_build_info(cl_program program, cl_device_id device, cl_program_build_info name,
                              size_t value_bytes, void *value, size_t *size_ret);
    cl_int kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                  cl_kernel_work_group_info name, size_t value_bytes, void *value,
                                  size_t *size_ret);

    /* ---- transfers the device makes on its own ------------------------- */

    /* Nothing crosses but the two handles and the offsets, so these are
     * enqueued as they are and the host's own event answered. */
    cl_int enqueue_copy_buffer(cl_command_queue queue, cl_mem from, cl_mem to, size_t from_offset,
                               size_t to_offset, size_t bytes, cl_event *event, const ClWaitList &waits = ClWaitList());
    /* The pattern is at most 128 bytes (the API's own bound) and crosses with
     * the command; the host copies it before it answers. */
    cl_int enqueue_fill_buffer(cl_command_queue queue, cl_mem buffer, const void *pattern,
                               size_t pattern_bytes, size_t offset, size_t bytes, cl_event *event,
                               const ClWaitList &waits = ClWaitList());
    cl_int enqueue_copy_image(cl_command_queue queue, cl_mem from, cl_mem to,
                              const size_t *from_origin, const size_t *to_origin,
                              const size_t *region, cl_event *event, const ClWaitList &waits = ClWaitList());

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
                              void *into, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_write_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                               const size_t *region, size_t row_pitch, size_t slice_pitch,
                               const void *from, cl_event *event, const ClWaitList &waits = ClWaitList());
    /* A buffer's rectangle, as an image's region is moved: the queue drained,
     * then commands that fit, so a read is complete when it returns whatever
     * the application asked for - which the API allows. */
    cl_int enqueue_read_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect, void *into,
                                    cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int enqueue_write_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                     const void *from, cl_event *event, const ClWaitList &waits = ClWaitList());
    /* Nothing crosses but handles and the geometry: enqueued as it is, the
     * host refusing what the API refuses. */
    cl_int enqueue_copy_buffer_rect(cl_command_queue queue, cl_mem from, cl_mem to, const size_t *from_origin,
                                    const size_t *to_origin, const size_t *region, size_t from_row_pitch,
                                    size_t from_slice_pitch, size_t to_row_pitch, size_t to_slice_pitch,
                                    cl_event *event, const ClWaitList &waits = ClWaitList());
    /* The three sampler properties and nothing else; the deprecated
     * clCreateSampler is the same list written out. */
    cl_sampler create_sampler(cl_context context, const uint64_t *properties, cl_int *error);
    /* clGetSamplerInfo: the count and the context this ICD's, the rest the
     * host's answer about its sampler. */
    cl_int     sampler_info(cl_sampler sampler, cl_sampler_info name, size_t value_bytes, void *value,
                            size_t *size_ret);
    /* The host's formats for an image of `type`: a format holds no handle, so
     * the answer is the host's as it stands. */
    cl_int supported_image_formats(cl_context context, cl_mem_flags flags, cl_mem_object_type type,
                                   cl_uint num_entries, cl_image_format *formats, cl_uint *num_formats);

    /* ---- cl_khr_gl_sharing ------------------------------------------------ */

    /* An OpenCL object over an OpenGL one of the context `context` was made
     * from, named as the application names it in GL - the host resolves the
     * name in that context's share group. CL_INVALID_CONTEXT for a context not
     * made from a GL context; the host's own refusal otherwise. Every texture
     * spelling - clCreateFromGLTexture and the 1.1 pair - is one call.
     *
     * The calling thread's GL context's held calls are run by the host first
     * (GlShareSource::run_held_calls): our OpenGL ICD sends a context's calls
     * a batch at a time, and the host runs each context on a worker of its
     * own, so a texture an application named with glGenTextures and made with
     * a bind still held did not exist there yet - DaVinci Resolve's viewer
     * textures, made on the thread that registers them, were refused as
     * CL_INVALID_GL_OBJECT ("Your GPU memory is full"). */
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
    /* clGetGLContextInfoKHR, answered here: every OpenGL context of ours runs
     * on the adapter the primary display is on, which is the first device. */
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

    /* clGetDeviceIDsFromD3D10KHR: the device on the avgpu adapter a DXGI
     * adapter or an ID3D10Device is - preferred and all alike, since one
     * adapter is one device - and CL_DEVICE_NOT_FOUND for any other adapter,
     * which is how an application tells which of its adapters a device of
     * ours belongs to. */
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

    /* ---- cl_khr_d3d11_sharing (docs/d3d11-sharing.md) -------------------- */

    /* The extension's numbers (cl_d3d11.h), which portable code cannot
     * include: it includes d3d11.h. */
    static constexpr cl_uint d3d11_device_source = 0x4019u;
    static constexpr cl_uint d3d11_adapter_source = 0x401Au;
    static constexpr cl_uint d3d11_preferred_devices = 0x401Bu;
    static constexpr cl_uint d3d11_all_devices = 0x401Cu;
    static constexpr cl_uint d3d11_context_device = 0x401Du;
    static constexpr cl_uint d3d11_resource_info = 0x401Eu;
    static constexpr cl_uint d3d11_subresource_info = 0x401Fu;
    static constexpr cl_uint d3d11_prefer_shared = 0x402Du;
    static constexpr cl_int  invalid_d3d11_device = -1006;
    static constexpr cl_int  invalid_d3d11_resource = -1007;

    /* clGetDeviceIDsFromD3D11KHR, answered as Direct3D 10's is. */
    cl_int device_ids_from_d3d11(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                 cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices);
    /* A cl_mem over an ID3D11Buffer of the device `context` was made from: an
     * ordinary buffer of the host's, its size, which acquire and release copy
     * the resource's bytes into and back out of. */
    cl_mem create_from_d3d11_buffer(cl_context context, cl_mem_flags flags, void *resource, cl_int *error);
    /* The same over one subresource of an ID3D11Texture2D or ID3D11Texture3D,
     * `kind` saying which the application called for: an image of that
     * subresource's mip level, in the format the extension maps its DXGI format
     * to. */
    cl_mem create_from_d3d11_texture(cl_context context, cl_mem_flags flags, void *resource, cl_uint subresource,
                                     D3d11Resource::Kind kind, cl_int *error);
    /* Acquire copies each object's resource in; release drains the queue and
     * copies each back. Both wait for the wait list first, and both are done
     * when they return, so the event they answer is a marker. */
    cl_int enqueue_acquire_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                 const cl_event *events, cl_event *event);
    cl_int enqueue_release_d3d11(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                 const cl_event *events, cl_event *event);

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
                                      cl_event *event, const ClWaitList &waits = ClWaitList());

    /* ---- the rest of the enqueue surface (cl_icd_root_enqueue.cpp) ------ */

    /* clEnqueueTask: an NDRange of one work-item in a group of one. */
    cl_int enqueue_task(cl_command_queue queue, cl_kernel kernel, cl_event *event,
                        const ClWaitList &waits = ClWaitList());
    /* The 1.1 marker, which always hands an event back: CL_INVALID_VALUE for
     * nowhere to put it. */
    cl_int enqueue_event_marker(cl_command_queue queue, cl_event *event);
    /* clEnqueueWaitForEvents: a barrier behind `count` events, which must
     * name at least one. */
    cl_int enqueue_wait_for_events(cl_command_queue queue, cl_uint count, const cl_event *events);
    /* An image's region into a buffer and back, and a fill of an image's
     * region: handles and geometry, run by the host behind the wait list. A
     * fill colour is four components of at most four bytes, which is what
     * every format but a depth one fills with - and this ICD reports no depth
     * images. */
    cl_int enqueue_copy_image_to_buffer(cl_command_queue queue, cl_mem image, cl_mem buffer, const size_t *origin,
                                        const size_t *region, size_t offset, cl_event *event,
                                        const ClWaitList &waits = ClWaitList());
    cl_int enqueue_copy_buffer_to_image(cl_command_queue queue, cl_mem buffer, cl_mem image, size_t offset,
                                        const size_t *origin, const size_t *region, cl_event *event,
                                        const ClWaitList &waits = ClWaitList());
    cl_int enqueue_fill_image(cl_command_queue queue, cl_mem image, const void *colour, const size_t *origin,
                              const size_t *region, cl_event *event, const ClWaitList &waits = ClWaitList());
    /* A hint where memory should live next, which the host takes as it will:
     * every object must be one of the queue's context. */
    cl_int enqueue_migrate(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                           cl_mem_migration_flags flags, cl_event *event, const ClWaitList &waits = ClWaitList());
    /* A region of a buffer, which holds one of the application's references on
     * the buffer for its life. A buffer the arena backs, or one over the
     * application's memory, lends its region of either to the sub-buffer, so
     * its maps are what the buffer's are. A sub-buffer of a sub-buffer is
     * CL_INVALID_MEM_OBJECT, as the API says. */
    cl_mem create_sub_buffer(cl_mem buffer, cl_mem_flags flags, cl_buffer_create_type type, const void *info,
                             cl_int *error);
    /* An image's region staged in a block of the ICD's own, read in at the map
     * and written back at the unmap, as a buffer's staged map is: the wire
     * carries no host map of an image, and needs none. Its rows and slices
     * are packed, and the pitches answered say so. Complete when it returns,
     * which the API allows of any map. */
    void  *map_image(cl_command_queue queue, cl_mem image, cl_bool blocking, cl_map_flags flags,
                     const size_t *origin, const size_t *region, size_t *row_pitch, size_t *slice_pitch,
                     cl_event *event, cl_int *error, const ClWaitList &waits = ClWaitList());
    /* clCreateImageWithProperties: 3.0 defines no image property, so the list
     * may only be empty, and the image is clCreateImage's. */
    cl_mem create_image_with_properties(cl_context context, const cl_mem_properties *properties,
                                        cl_mem_flags flags, const cl_image_format *format, const cl_image_desc *desc,
                                        void *host_pointer, cl_int *error);
    /* A pipe is memory the host makes, and whether a device has them at all is
     * its own CL_DEVICE_PIPE_SUPPORT; 3.0 defines no pipe property either. */
    cl_mem create_pipe(cl_context context, cl_mem_flags flags, cl_uint packet_bytes, cl_uint packets,
                       const cl_pipe_properties *properties, cl_int *error);
    cl_int pipe_info(cl_mem pipe, cl_pipe_info name, size_t value_bytes, void *value, size_t *size_ret);
    /* Shared memory copied, filled, freed and migrated on a queue. A copy
     * between two allocations is the host's; one with the application's own
     * memory on either side goes through a buffer of the ICD's, mapped on the
     * host, whose address the host takes as shared memory - no command on this
     * wire carries bytes to a shared address otherwise. */
    cl_int svm_memcpy(cl_command_queue queue, cl_bool blocking, void *to, const void *from, size_t bytes,
                      cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int svm_mem_fill(cl_command_queue queue, void *pointer, const void *pattern, size_t pattern_bytes,
                        size_t bytes, cl_event *event, const ClWaitList &waits = ClWaitList());
    using SvmFreed = void(CL_CALLBACK *)(cl_command_queue queue, cl_uint count, void *pointers[], void *user_data);
    /* Behind everything queued and the list: the allocations go then, or the
     * application's callback is handed them to free itself. */
    cl_int enqueue_svm_free(cl_command_queue queue, cl_uint count, void *pointers[], SvmFreed freed,
                            void *user_data, cl_event *event, const ClWaitList &waits = ClWaitList());
    /* Another hint, which every allocation here would ignore: a point in the
     * queue behind the list. */
    cl_int enqueue_svm_migrate(cl_command_queue queue, cl_uint count, const void **pointers,
                               cl_event *event, const ClWaitList &waits = ClWaitList());

    /* ---- maps and shared virtual memory (US2) --------------------------- */

    /* A pointer the application may read and write, staged in guest memory
     * and filled from the buffer; null with the API's own error. */
    void  *map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking, cl_map_flags flags,
                      size_t offset, size_t bytes, cl_event *event, cl_int *error, const ClWaitList &waits = ClWaitList());
    /* Writes a staged mapping back where the application asked to write, then
     * gives it up. CL_INVALID_VALUE for a pointer no live mapping answered. */
    cl_int unmap(cl_command_queue queue, cl_mem buffer, void *pointer, cl_event *event, const ClWaitList &waits = ClWaitList());

    /* Coarse-grained shared virtual memory: the application is handed a
     * mirror, and the host address it stands for is what crosses the wire. */
    void  *svm_alloc(cl_context context, cl_svm_mem_flags flags, size_t bytes, cl_uint alignment);
    void   svm_free(cl_context context, void *pointer);
    /* Fills the mirror from the host; an unmap writes it back. Host access to
     * a coarse-grained allocation is defined only between the two. */
    cl_int svm_map(cl_command_queue queue, cl_bool blocking, cl_map_flags flags, void *pointer,
                   size_t bytes, cl_event *event, const ClWaitList &waits = ClWaitList());
    cl_int svm_unmap(cl_command_queue queue, void *pointer, cl_event *event, const ClWaitList &waits = ClWaitList());
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
    /* The same for a query, naming which was asked. */
    cl_int   noted_query(const char *entry_point, uint32_t query, cl_int result);
    /* clGetContextInfo, clGetMemObjectInfo and clGetImageInfo: an answer that
     * names an object is this ICD's handle for it; the rest is the host's. */
    cl_int   context_info(cl_context context, cl_context_info name, size_t value_bytes, void *value,
                          size_t *size_ret);
    /* clGetCommandQueueInfo: the context, the device and the count are this
     * ICD's own; no queue here has a default device queue; the rest is the
     * host's answer about its queue. */
    cl_int   queue_info(cl_command_queue queue, cl_command_queue_info name, size_t value_bytes, void *value,
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
    /* The host's answer to a device query, kept the first time and given
     * from the guest after: a device's properties never change. */
    cl_int           host_device_info(ClientTransport &wire, ClDevice &device, cl_device_info name,
                                      size_t value_bytes, void *value, size_t *size_ret);
    /* The host's answer to a mem query, kept and given from the guest after
     * for the five that cannot change once the object is made. */
    cl_int           host_mem_info(ClientTransport &wire, ClObject &object, cl_mem_info name, size_t value_bytes,
                                   void *value, size_t *size_ret);
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
    cl_int           import_handle_types(ClDevice &device, size_t value_bytes, void *value,
                                         size_t *size_ret);
    cl_int           device_extensions(ClDevice &device, size_t value_bytes, void *value,
                                       size_t *size_ret);
    /* The host device's own CL_DEVICE_EXTENSIONS into `words`. */
    cl_int           host_extensions(ClDevice &device, ExtensionWords &words);
    /* CL_DEVICE_EXTENSIONS_WITH_VERSION: the same intersection, each with the
     * version this ICD carries. */
    cl_int           device_extension_versions(ClDevice &device, size_t value_bytes, void *value,
                                               size_t *size_ret);
    /* CL_DEVICE_OPENCL_C_FEATURES: the host compiler's, less what needs a
     * device queue, which no queue here is. */
    cl_int           c_features(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret);
    /* CL_PLATFORM_EXTENSIONS_WITH_VERSION: the platform-wide rows. */
    cl_int           platform_extension_versions(size_t value_bytes, void *value, size_t *size_ret) const;
    /* CL_DEVICE_GLOBAL_FREE_MEMORY_AMD, what AdapterMemory leaves of the local budget. */
    cl_int           free_memory(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret);
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
     * transfer that asks the host to block does first. `driven`: the queue's
     * context is one the driver wakes (GuestClContext::wakes). `waits` are the
     * host's numbers for the `count` events the command after it was told to
     * wait for, which the marker waits for too: on an in-order queue it is
     * reached once both have been. */
    cl_int           drain(ClientTransport &wire, uint64_t queue, bool driven, const uint64_t *waits,
                           uint32_t count);
    /* What a transfer the host is made to block on does before it is sent:
     * the drain, behind the application's wait list where it named one, so
     * the host's call is behind nothing. The list judged as the API judges
     * it; a queue known idle with no list needs nothing. */
    [[nodiscard]] cl_int settle(ClientTransport &wire, ClObject &queue, const ClWaitList &waits);
    /* `listed` (the host's numbers for `waits`) without the application's
     * user events not yet set, for a call that returns before its work is
     * done; how many are left. */
    uint32_t             without_unset_user_events(ClientTransport &wire, const ClWaitList &waits, uint64_t *listed);
    /* The adapter's vendor, name and PCI location, to the device's report and
     * extension policy; its name into `name`. */
    void             describe_adapter(uint32_t adapter, ClDevice &device);
    /* Learns the devices, once: which avgpu adapters there are and what
     * Windows calls each, and the platform's name from them - asked again
     * while one device has no name. No device is opened. Under lock_. */
    void             learn_devices();
    /* The host's handle for the device the context of `object` is on, which
     * is what a command naming a device sends; 0 when it has none. */
    uint64_t         host_device_of(ClObject &object);
    /* clGetDeviceIDsFromD3D10KHR's and 11's answer for an object on the adapter
     * of `luid`: that adapter's device, opened, or CL_DEVICE_NOT_FOUND. */
    cl_int           device_ids_for_luid(uint64_t luid, cl_device_id *devices, cl_uint *num_devices);
    /* The ID3D11Device `list` names, into `device`: null for none, and
     * CL_INVALID_D3D11_DEVICE_KHR for an object that is not one or is on
     * another adapter than `on`'s. */
    cl_int           d3d11_device_for(const ClPropertyList &list, const ClDevice &on, void *&device);
    /* What a create over `resource` is judged by: `context` made from a
     * Direct3D 11 device, `flags` only the three access flags, `resource` of
     * `kind` and made on that device with `subresource` one of its, and no
     * cl_mem over it yet. Its owner and description into `owner` and
     * `described`. */
    cl_int           d3d11_resource(cl_context context, cl_mem_flags flags, void *resource, uint32_t subresource,
                                    D3d11Resource::Kind kind, GuestClContext *&owner, D3d11Resource &described);
    /* `made` kept as the object over `subresource` of `resource`, which it now
     * holds a reference to; given back, and null with the error, when it
     * cannot be kept. */
    cl_mem           d3d11_object(cl_mem made, GuestClContext &owner, void *resource, uint32_t subresource,
                                  const D3d11Resource &described, cl_mem_flags flags, cl_int *error);
    /* An image over a shared twin of `subresource` of `resource`, opened in
     * `context` (docs/d3d11-sharing.md, stage 2), with the twin in `twin`;
     * null, and no twin, when the device will not share one or the host
     * cannot open it as an image - which leaves the object to stage 1. */
    cl_mem           d3d11_twin(cl_context context, GuestClContext &owner, void *resource, uint32_t subresource,
                                const D3d11Resource &described, void *&twin);
    /* The queue an acquire or release names, its context made from Direct3D 11
     * and still able to send, and its wait list waited for. */
    cl_int           d3d11_queue(cl_command_queue queue, cl_uint waits, const cl_event *events, ClObject *&held,
                                 GuestClContext *&context, ClientTransport *&wire);
    /* Each object's resource copied into it, and each copied back: the
     * objects of `context`, judged by the caller. */
    cl_int           d3d11_copy_in(cl_command_queue queue, GuestClContext &context, const cl_mem *objects,
                                   cl_uint count);
    cl_int           d3d11_copy_out(cl_command_queue queue, GuestClContext &context, const cl_mem *objects,
                                    cl_uint count);
    /* The object `mem` is over a Direct3D 11 resource as, or null. */
    D3d11Object     *d3d11_object_of(ClObject &mem);
    /* The same, for `mem` in `owner`, looked up under lock_. */
    D3d11Object     *d3d11_object_in(GuestClContext &owner, const void *mem);
    /* A drain, unless the queue is known idle; idle after one (ClObject). */
    /* `queue` known idle, and its context's retired memory given back when
     * every queue of it is (GuestClContext::reclaim). */
    void             drained_queue(ClObject &queue);
    /* A released buffer's arena block or lent pages: given back now when no
     * queue of `context` may still run work over them, retired otherwise. */
    void             let_go_of_memory(GuestClContext &context, ClObject &buffer);
    [[nodiscard]] cl_int drain_unless_idle(ClientTransport &wire, ClObject &queue);
    /* The queue, looked up for a command that leaves work on it: it is no
     * longer known idle. */
    ClObject        *busied(cl_command_queue queue);
    /* The event a transfer that completed inside its call hands back when one
     * was asked for: a marker in the queue, which is complete as soon as the
     * host looks. CL_SUCCESS with nothing made when `event` is null. */
    cl_int           marker_event(ClientTransport &wire, ClObject &queue, cl_event *event);
    /* The host's numbers for an application's wait list, into `into`, which
     * points into `room`: CL_INVALID_EVENT_WAIT_LIST for a list the API
     * refuses or an event of another context, CL_OUT_OF_HOST_MEMORY for one
     * there is no memory to hold. */
    cl_int           wait_list(GuestClContext &context, cl_uint count, const cl_event *events, ClHostNumbers &room,
                               uint64_t *&into);
    /* The share name of the OpenGL context `list` names, into `share`: 0 for a
     * list naming none, CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR for one that
     * names a context this process's OpenGL ICD cannot vouch for. */
    cl_int           gl_share_for(const ClPropertyList &list, uint32_t &share);
    /* The context object `context` names when it was made from a GL context
     * and can still send; null with the error set otherwise. */
    ClObject        *gl_sharing(cl_context context, cl_int *error);
    void             note_build_log(ClientTransport &wire, uint64_t program, uint64_t device, const char *options);
    /* The API's judgement of a program's device list on `context`: none, or
     * the context's one device; CL_INVALID_VALUE or CL_INVALID_DEVICE
     * otherwise. */
    cl_int           program_devices(ClObject &context, cl_uint num_devices, const cl_device_id *device_list);
    /* The host's numbers for `count` programs of `context`, into `into`,
     * which points into `room`: CL_INVALID_VALUE for a list the API refuses,
     * CL_INVALID_PROGRAM for one not a program of `context`,
     * CL_OUT_OF_HOST_MEMORY for one there is no memory to hold. */
    cl_int           program_list(ClObject &context, cl_uint count, const cl_program *programs, ClHostNumbers &room,
                                  uint64_t *&into);
    /* CL_PROGRAM_BINARIES into the application's pointers. */
    cl_int           program_binaries(ClientTransport &wire, ClObject &program, size_t value_bytes, void *value,
                                      size_t *size_ret);
    /* What a host create over a GL object answered, wrapped as a mem object. */
    cl_mem           gl_object(ClientTransport &wire, ClObject &context, uint64_t made, cl_int host_error,
                               cl_int *error);
    using GlObjectsCommand = cl_int (*)(ClientTransport &, uint64_t, cl_uint, const uint64_t *, cl_uint,
                                        const uint64_t *, uint64_t *);
    cl_int           enqueue_gl_objects(GlObjectsCommand command, cl_command_queue queue, cl_uint count,
                                        const cl_mem *objects, cl_uint waits, const cl_event *events,
                                        cl_event *event);
    /* The write-back of an image's staged mapping, behind its queue, and the
     * mapping given up. */
    cl_int           unmap_image(ClientTransport &wire, ClObject &queue, uint64_t image, MappedRegion &region,
                                 cl_event *event);
    /* `bytes` of the application's memory at `from` copied to the host's
     * shared address `to`, and from `from` back into `to`, through a buffer
     * of the ICD's own mapped on the host - an address the host takes as
     * shared memory. On `queue`, which the caller has settled. */
    cl_int           svm_from_guest(ClientTransport &wire, ClObject &queue, uint64_t to, const void *from,
                                    size_t bytes);
    cl_int           svm_to_guest(ClientTransport &wire, ClObject &queue, void *to, uint64_t from, size_t bytes);
    /* The host's event for a command that was asked for one, wrapped as the
     * application's when it wanted one and given back when it did not. */
    void             hand_over(ClientTransport &wire, ClObject &queue, uint64_t produced, cl_event *event);
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
    cl_int           fill_image(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t device,
                                uint64_t image, const cl_image_desc &desc, const void *from);
    /* The same for a buffer whose bytes one create could not carry. */
    cl_int           fill_buffer(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t device,
                                 uint64_t buffer, size_t bytes, const void *from);
    /* A host queue on `context` and `device` that nothing else has, for a
     * fill: nothing is ahead of its writes for the host to wait behind. 0,
     * with the error, when the host will not make one. */
    uint64_t         own_queue(ClientTransport &wire, uint64_t context, uint64_t device, cl_int &error);
    /* Opens `device`, once, on the first ask: the ICD's own context on its
     * adapter, and the host's handle for its GPU. Under lock_. */
    [[nodiscard]] Status open_device(ClDevice &device);
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
    /* Under lock_: one of the application's references to `held`, which is
     * `object`, let go - with the last, the host's object, and the context
     * once nothing holds it - and the destructor callbacks that came due
     * moved to `due`, to run once the lock is let go. The buffer a
     * sub-buffer's last reference held, to release then too; null for none. */
    ClObject        *drop_reference(void *object, ClObject &held, ClObject::Kind kind, ClDestructorCallbacks::Due &due);
    /* `held`'s count of the application's references, read under lock_:
     * another thread may be retaining or releasing it. */
    cl_uint          reference_count(const ClObject &held);

    ThreadLock            &lock_;
    LogSink          &log_;
    GlShareSource         &gl_share_;
    D3dAdapterSource      &d3d_adapters_;
    D3d11Bridge           &d3d11_;
    ClIcdBinding          &binding_;
    ClDispatchTable        dispatch_;
    ClPlatform             platform_;
    ClObjectTable          objects_;
    /* What the adapters are found with. */
    Kmt                   &kmt_;
    /* Before the contexts, which hold links on the devices' connections. */
    ClDevices              devices_;
    /* The NT handles this process holds for shared objects, which every
     * context's imports resolve through (feature 053). */
    icd::SharedHandles     shared_handles_;
    ObjectPool<GuestClContext> contexts_;
    BlockSource           &memory_;
    ThreadLock            &send_lock_;
    ThreadLock            &blocks_lock_;
    ClEventWatcher         watcher_;
    ClBlockingWait         waits_;
    /* The host's device answers, keyed by the device they are about, each
     * as long as the longest - the extension list. */
    ClInfoAnswers<host_extensions_bytes> device_answers_;
    ClPendingReads         reads_;
    /* The ICD's lines said once: every refused call, and what else
     * ClRefusalLog names. */
    KeyedLog               lines_;
    icd::RefusedCalls      refused_;
    ClRefusalLog           refusals_;
    /* Under lock_, as the object releases that make them due are. */
    ClDestructorCallbacks  destructor_callbacks_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_ICD_ROOT_HPP
