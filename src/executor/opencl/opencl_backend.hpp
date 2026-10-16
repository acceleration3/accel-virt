/* One guest context's OpenCL, run on the host's own.
 *
 * The generated `RealDispatch` already forwards all 105 commands the profile
 * carries into whatever OpenCL this process loaded, so what this adds is the
 * three answers the ABI needs: whether an id is carried, whether a frame
 * decoded, and whether the host lost its device. */
#ifndef AV_OPENCL_BACKEND_HPP
#define AV_OPENCL_BACKEND_HPP

#include "executor/opencl/cl_event_signals.hpp"
/* The generated server defines what OpenCL's headers are included with, so it
 * comes before anything else that includes them. */
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "executor/opencl/cl_argument_retention.hpp"
#include "executor/opencl/cl_binary_envelope.hpp"
#include "executor/opencl/cl_program_il.hpp"
#include "executor/opencl/spirv_argument_kinds.hpp"
#include "executor/opencl/cl_bounced_rect.hpp"
#include "executor/opencl/cl_read_ahead.hpp"
#include "executor/opencl/copied_gl_sharing.hpp"
#include "executor/opencl/direct_gl_sharing.hpp"
#include "executor/guest_region.hpp"
#include "proto/wire/guest_bytes.hpp"
#include "executor/host_object_registry.hpp"
#include "executor/opencl/host_svm_ranges.hpp"
#include "executor/opencl/kernel_pacer.hpp"
#include "executor/opencl/staging_bytes.hpp"
#include "executor/session_backend.hpp"
#include "executor/shim/host_heap.hpp"
#include "executor/opengl/gl_context_attributes.hpp"
#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_object_bytes.hpp"
#include "executor/shim/gl_renderbuffer_texture.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "proto/generated/opencl/cl_real_dispatch.h"

#include <stdint.h>

namespace av {

class SharedImageShape;

class OpenGlBackend;

class GlDisplay;
class ImportTable;
class OpenClDevice;
class SourceMemory;
/* What the tests reach the backend's private state through: the copy path
 * taken on a host whose OpenCL would have shared, and the arena's refusal. */
struct OpenClBackendProbe;

class OpenClBackend final : public clproto::RealDispatch, public SessionBackend {
public:
    /* Its tables grow from a heap of its own; a test hands it the source to
     * grow from instead. */
    OpenClBackend();
    explicit OpenClBackend(BlockSource &blocks);

    /* Binds this context to the platform and device the host found for its
     * GPU, which must outlive it. False for a null device: a GPU the host
     * found no OpenCL for runs nothing, so the context is refused at setup
     * rather than shown an empty platform list.
     *
     * `display` is the same GPU's EGL display, or null. A GL context on it
     * is the vehicle: what a descriptor another API exported is imported
     * through on a host whose OpenCL has no `cl_khr_external_memory`, the host
     * `cl_context` then made naming it, so `clCreateFromGLBuffer` reaches the
     * same allocation (feature 053, T019; research.md §21); and, with
     * `share`, the context in the guest's GL share group its GL objects are
     * reached through.
     *
     * Whether the host's OpenCL shares with the vehicle is measured here and
     * not assumed - a context is made against it and released, once, before
     * the guest asks. Mesa's and Intel's do, and the guest's GL objects are
     * then the host's own (DirectGlSharing). NVIDIA's makes no context against
     * any EGL context; there they are copies (CopiedGlSharing), and no
     * descriptor is imported. A null display, or one with no GL context to be
     * had, leaves this sharing and importing nothing, which the device answers
     * as refusals.
     *
     * `share` is the OpenGL context of the guest's that this one was made to
     * share objects with (cl_khr_gl_sharing), or null. With one, the vehicle
     * shares its lists, so a GL name the guest hands clCreateFromGLBuffer is
     * the same object here; and a context with no vehicle is refused, since
     * the guest asked for nothing else.
     *
     * `imports` is the session's: a buffer over lent pages names the import
     * it is made over. */
    [[nodiscard]] bool initialize(const OpenClDevice *device, const GlDisplay *display, const OpenGlBackend *share,
                                  const ImportTable &imports);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    /* OpenCL has no Vulkan device underneath to lose. */
    [[nodiscard]] bool lose_device() override;
    void end_batch() override;
    bool poll() override;
    void gpu_progress(uint64_t &marked, uint64_t &reached) const override;
    /* The context's arena first; every import after is pages the
     * application lent the device, which its CL_MEM_USE_HOST_PTR buffers are
     * made over (docs/host-memory.md, "Application memory"), named by their
     * memory's own address. */
    [[nodiscard]] bool adopt(const SourceMemory &memory, uint64_t &handle) override;
    void abandon(const SourceMemory &memory, uint64_t handle) override;

    /* This API exports no descriptor (feature 053,
     * contracts/wire_additions.md): only Vulkan does, so a guest asking this
     * one to export is refused by the device with a status rather than handed
     * an object nothing made. Importing is another matter - see below. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     int &fd) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     const SharedImageShape &shape,
                                     uint64_t &wire_handle, uint64_t &detail) override;
    void               release_object(uint32_t kind, uint64_t wire_handle, uint64_t detail) override;

    /* What the generated decoder asks before it hands the host's OpenCL a
     * handle, and tells it after: the object a guest's number names, and what
     * a call made, retained or released. */
    void *handle_for(clproto::HandleKind kind, uint64_t wire) const override;
    /* An address the guest names, as a pointer, or null unless it and the
     * bytes after it lie inside one allocation this context made or one
     * mapping one of its maps answered. Those are the only two kinds of host
     * address the guest is ever told, so they are the only two it may name. */
    void *svm_for(uint64_t address, uint64_t bytes) const override;
    void  made(clproto::HandleKind kind, void *object) override;
    bool  room_for(uint64_t objects) override;
    void  retained(clproto::HandleKind kind, void *object) override;
    void  released(clproto::HandleKind kind, void *object) override;

    /* The three calls that could name another GPU, answered from the bound
     * device rather than forwarded: the host's list has every platform and
     * device it has, and a context from a type could span every GPU of that
     * type on the platform (feature 044, T034). The API's own errors for the
     * arguments are kept - a null platform means the bound one, since the
     * guest is shown no other - and a context unbound, or bound to a device
     * that is not the type asked for, finds nothing. */
    cl_int     on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms,
                                   cl_uint *num_platforms) override;
    cl_int     on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries,
                                 cl_device_id *devices, cl_uint *num_devices) override;
    /* The host's answer, with the extension list the guest is shown in place
     * of the host's own: `cl_khr_gl_sharing` where GL
     * objects reach this GPU's OpenCL, `cl_khr_external_memory` where a
     * descriptor can be opened as a cl_mem. The guest's ICD reports both as
     * it reads them, so leaving either in where it fails would report an
     * extension whose every use fails (constitution VI; feature 053, T019).
     * Every other query is forwarded untouched. */
    cl_int     on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size,
                                  void *param_value, size_t *param_value_size_ret) override;
    cl_context on_clCreateContextFromType(const cl_context_properties *properties, cl_device_type device_type,
                                          cl_int *errcode_ret) override;
    /* Made by the context's GL sharing (GlSharing::make_context): where the
     * host shares, naming this backend's GL context, so a GL object of the
     * guest's or a buffer imported through OpenGL can become a `cl_mem`. The
     * guest never asks for that and never sees it (feature 053, T019). */
    cl_context on_clCreateContext(const cl_context_properties *properties, cl_uint num_devices,
                                  const cl_device_id *devices, cl_int *errcode_ret) override;

    /* The shared allocations, remembered so an address the guest hands back
     * can be judged; the host's own allocator answers the allocation itself. */
    void *on_clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size,
                        cl_uint alignment) override;
    void  on_clSVMFree(cl_context context, void *svm_pointer) override;


    /* A buffer whose memory is already the host's: the guest names an offset
     * of the context's arena through ClBlobBacking, and the host's cl_mem is
     * made over that memory with CL_MEM_USE_HOST_PTR, so a map costs no copy.
     * An offset that does not lie inside the arena is CL_INVALID_VALUE, and a
     * list without the property is the ordinary call. */
    cl_mem on_clCreateBufferWithProperties(cl_context context, const cl_mem_properties *properties,
                                           cl_mem_flags flags, size_t size, void *host_ptr,
                                           cl_int *errcode_ret) override;

    /* A map answers a host address, and the unmap that ends it names that
     * address back - so the decoder has to be able to judge it, exactly as it
     * judges an SVM address. Kept apart from the allocations on purpose: they
     * share a decoder seam but not a lifetime, and one table would let
     * clSVMFree be handed a mapping's pointer and free it. */
    /* A buffer released while mapped takes its mappings with it: nothing names
     * their addresses again, and each holds a row of mapped_. */
    cl_int on_clReleaseMemObject(cl_mem memobj) override;
    void  *on_clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
                                 cl_map_flags map_flags, size_t offset, size_t size,
                                 cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                 cl_event *event, cl_int *errcode_ret) override;
    cl_int on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void *mapped_ptr,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list, cl_event *event) override;

    /* A transfer's host memory is the decode arena, which the next command
     * decodes over, or guest memory the guest lent (guest_bytes), which it may
     * give to another transfer once this one is answered - and a host driver
     * may touch a non-blocking transfer's memory when its queue reaches it
     * rather than at the call (rusticl does). So every transfer is made
     * blocking here, whatever the guest asked: the bytes have moved before the
     * answer. The guest's ICD drains the queue first, so the host blocks on
     * the copy and not on work ahead of it. */
    /* A read with no events into the decode arena: the guest's pieces, from
     * one read made ahead of them (ClReadAhead). A read into lent memory is
     * one piece already, and reading ahead would only copy it twice. */
    cl_int on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                                  size_t offset, size_t size, void *ptr, cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list, cl_event *event) override;
    /* A kernel goes to the host GPU in a group no longer than KernelPacer
     * allows, and a flush or a finish ends the group the guest gathered. */
    cl_int on_clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim,
                                     const size_t *global_work_offset, const size_t *global_work_size,
                                     const size_t *local_work_size, cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel, cl_uint num_events_in_wait_list,
                            const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clFlush(cl_command_queue command_queue) override;
    cl_int on_clFinish(cl_command_queue command_queue) override;
    cl_int on_clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write,
                                   size_t offset, size_t size, const void *ptr,
                                   cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                   cl_event *event) override;
    cl_int on_clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read,
                                 const size_t *origin, const size_t *region, size_t row_pitch,
                                 size_t slice_pitch, void *ptr, cl_uint num_events_in_wait_list,
                                 const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueReadBufferRect(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                                      const size_t *buffer_origin, const size_t *host_origin,
                                      const size_t *region, size_t buffer_row_pitch,
                                      size_t buffer_slice_pitch, size_t host_row_pitch,
                                      size_t host_slice_pitch, void *ptr, cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_write,
                                  const size_t *origin, const size_t *region, size_t input_row_pitch,
                                  size_t input_slice_pitch, const void *ptr,
                                  cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                  cl_event *event) override;
    cl_int on_clEnqueueWriteBufferRect(cl_command_queue command_queue, cl_mem buffer,
                                       cl_bool blocking_write, const size_t *buffer_origin,
                                       const size_t *host_origin, const size_t *region,
                                       size_t buffer_row_pitch, size_t buffer_slice_pitch,
                                       size_t host_row_pitch, size_t host_slice_pitch, const void *ptr,
                                       cl_uint num_events_in_wait_list,
                                       const cl_event *event_wait_list, cl_event *event) override;

    /* A program's binaries cross sealed (ClBinaryEnvelope): their sizes are
     * the sealed sizes, and CL_PROGRAM_BINARIES' value on the wire is the
     * sealed binaries back to back in the program's device order - never the
     * array of pointers the API's own value is, which would be guest bytes
     * the host wrote through. Every other query is the host's. */
    cl_int     on_clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size,
                                   void *param_value, size_t *param_value_size_ret) override;
    /* A binary is opened before the host sees it, and one this device and
     * driver did not seal, or one whose kernels would not say what their
     * arguments are (ClArgumentRetention), is CL_INVALID_BINARY. One device,
     * the bound one, so the one status the wire carries is that device's. */
    cl_program on_clCreateProgramWithBinary(cl_context context, cl_uint num_devices, const cl_device_id *device_list,
                                            const size_t *lengths, const unsigned char **binaries,
                                            cl_int *binary_status, cl_int *errcode_ret) override;
    /* The two names this carries: the SVM pointers, which the decoder
     * resolved, and fine-grained system SVM, which the guest's device does
     * not have (its CL_DEVICE_SVM_CAPABILITIES is coarse buffers alone), so
     * only turning it off reaches the host. Any other name is a value the
     * host would read as it pleases, and is refused. */
    cl_int     on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size,
                                      const void *param_value) override;
    /* A kernel of a program made from IL: what its module declares the
     * argument to be, where the host keeps less (SpirvArgumentKinds). */
    bool       declared_argument(cl_kernel kernel, cl_uint index, clproto::KernelArgument &argument) override;

    /* cl_khr_gl_sharing, answered by the context's GlSharing; a context that
     * has none was made from no GL context, so a create or an acquire on it is
     * CL_INVALID_CONTEXT and a GL object query CL_INVALID_GL_OBJECT. The 1.1
     * texture creates are clCreateFromGLTexture's, as they are in OpenCL. */
    cl_mem on_clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj,
                                   cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                    cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                      cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint miplevel,
                                      cl_GLuint texture, cl_int *errcode_ret) override;
    cl_mem on_clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                         cl_int *errcode_ret) override;
    cl_int on_clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *gl_object_type,
                                cl_GLuint *gl_object_name) override;
    cl_int on_clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info param_name, size_t param_value_size,
                                 void *param_value, size_t *param_value_size_ret) override;
    cl_int on_clEnqueueAcquireGLObjects(cl_command_queue command_queue, cl_uint num_objects,
                                        const cl_mem *mem_objects, cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list, cl_event *event) override;
    cl_int on_clEnqueueReleaseGLObjects(cl_command_queue command_queue, cl_uint num_objects,
                                        const cl_mem *mem_objects, cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list, cl_event *event) override;
    /* No guest function pointer crosses the wire, so the callback registered
     * here is this backend's own: it signals the device, which wakes the
     * context's worker, and the guest asks what completed. Refused when the
     * device gave this library nothing to signal with - an older device - or
     * when this context is already watching as many events as it can. */
    cl_int on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type) override;
    /* An event the guest finds still CL_QUEUED has its queue flushed before
     * the answer goes back. The guest learns of completions only by asking
     * this and by the callback above, and neither moves a host queue along:
     * a marker or a transfer nothing flushed - the event a blocking read
     * hands back, or one a clWaitForEvents owes an implicit flush - stays
     * queued on a host that submits at flushes alone (rusticl), and the
     * guest waits forever. Kernels need no such help; pace() flushes them. */
    cl_int on_clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size, void *param_value,
                             size_t *param_value_size_ret) override;

    void bind_signal(void (*signal)(void *context), void *context) override;

    /* Releases what the guest never did. A context that dies owning a context
     * or a queue must take it with it. */
    void     shut_down() override;
    uint64_t ran() const;

private:
    /* The image over the imported `buffer` that `shape` describes, holding the
     * buffer's reference; the buffer itself where its memory is no image
     * OpenCL names. */
    cl_mem image_over(cl_mem buffer, const SharedImageShape &shape);
    friend struct OpenClBackendProbe;

    /* Holds `object`, which a call just answered, with one reference. One
     * already held as this kind is unchanged: enumeration answers the same
     * platform every time without a reference. conflict when the address is
     * held as another kind - the host could no longer say which object a
     * guest's number means - and no_memory when no room was made for it and
     * none can be. Null holds nothing. */
    [[nodiscard]] Status hold(clproto::HandleKind kind, void *object);
    /* `times` of the guest's references on `object` given back to the ICD,
     * as a dying context's are. Platforms are never released: the API has no
     * call for it. */
    void                 let_go(clproto::HandleKind kind, void *object, uint32_t times);

    /* The program's devices and each one's host binary size, at most
     * max_program_devices; 0 for a program the host will not describe. */
    cl_uint program_binary_sizes(cl_program program, size_t *sizes);
    /* CL_PROGRAM_BINARIES as the wire carries it (on_clGetProgramInfo). */
    cl_int  sealed_binaries(cl_program program, size_t param_value_size, void *param_value,
                            size_t *param_value_size_ret);

    /* CL_SUCCESS for a type the bound device is, which is always a GPU;
     * CL_INVALID_DEVICE_TYPE for no type or a bit OpenCL does not define;
     * CL_DEVICE_NOT_FOUND for any other. */
    cl_int answers_type(cl_device_type device_type) const;

    /* Whether this host will really make a context against `vehicle_` on
     * `display`: one is made on the bound device and released again. */
    [[nodiscard]] bool shares_with_gl(const GlDisplay &display) const;
    /* The guest's GL objects reached by copies from now on, and nothing
     * imported: what a host whose OpenCL will not share with the vehicle
     * gets. False, changing nothing, where the vehicle's GL cannot make the
     * copies. */
    [[nodiscard]] bool copy_gl_objects();
    /* The vehicle and what works through it, let go. */
    void               close_vehicle();
    /* The queue an acquire is enqueued on, made on the guest's context at the
     * first import: the acquire is the device's own bookkeeping and no guest
     * queue should carry it. Null when there is no context to make one on. */
    void *acquire_queue();
    /* After a kernel enqueued on `queue`: its group flushed when full, the
     * group before it on another queue flushed first. */
    void  pace(cl_command_queue queue);
    /* The timed group's marker, if it has completed. */
    void  look_at_marker();
    /* The queue and the marker the pacing holds, let go. */
    void  stop_pacing();

    /* A program here is the bound device's, but a binary's sizes are an
     * array per device, so they are bounded rather than assumed one. */
    static constexpr cl_uint max_program_devices = 16u;

    /* Where the tables below grow from: first, as they are destroyed after it. */
    HostHeap            heap_;
    BlockSource        &blocks_;
    const OpenClDevice *bound_;
    ClBinaryEnvelope    envelope_;
    ClArgumentRetention arguments_;
    /* The host's binaries on their way into their envelopes. */
    StagingBytes        binaries_;
    ClProgramIl         il_;
    SpirvArgumentKinds  argument_kinds_;
    /* Opened only when a display was given; a context nothing opened holds
     * nothing and closes to nothing. */
    GlContext             vehicle_;
    GlHostImport          import_;
    GlSharedBuffer        buffers_;
    GlRenderbufferTexture renderbuffer_textures_;
    GlObjectBytes         object_bytes_;
    DirectGlSharing       direct_;
    CopiedGlSharing       copied_;
    /* Which of the two this context's GL objects reach OpenCL through; null
     * for a context made from no GL context that imports nothing either. */
    GlSharing            *gl_;
    /* Whether a GL context could be had on this GPU, so a context made from
     * one of the guest's can share its objects one way or the other. */
    bool                  reaches_gl_;
    /* Whether a descriptor another API exported opens as a cl_mem here: the
     * host's OpenCL shares with the vehicle, whose GL imports memory. */
    bool                  imports_;
    /* The first context the guest made, which is the one a shared object is
     * opened into. An OpenCL application makes one; a second is a context this
     * cannot share with, and an open into it is refused rather than answered
     * with a cl_mem of the wrong context. */
    void              *context_;
    void              *queue_;
    /* What the guest holds, with the references it holds on each: OpenCL
     * counts them, and the ICD holds at least those, so an object is
     * forgotten no later than the ICD may free it. Held as its own address,
     * which is the number the guest names it by. */
    HostObjectRegistry<uint64_t, clproto::HandleKind, void *> handles_;
    HostSvmRanges    svm_;
    HostSvmRanges    mapped_;
    /* A context's first import: its blob arena, which the decoder resolves
     * lent parameters in. */
    GuestBytes       guest_;
    GuestRegion      arena_;
    /* The session's imports; null between sessions. */
    const ImportTable *imported_;
    ClEventSignals   events_;
    ClReadAhead      read_ahead_;
    /* Rectangles read into the arena where the host will not read into it. */
    ClBouncedRect    bounced_;
    KernelPacer      pacer_;
    /* The queue the group being gathered is on, retained while it is, and
     * the marker after the group being timed. */
    void            *paced_queue_;
    void            *marker_;
    uint64_t       ran_;
    bool           lost_;
};

}  // namespace av

#endif  // AV_OPENCL_BACKEND_HPP
