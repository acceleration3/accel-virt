/* One guest context's OpenCL, run on the host's own.
 *
 * The generated `RealDispatch` already forwards all 105 commands the profile
 * carries into whatever OpenCL this process loaded, so what this adds is the
 * three answers the ABI needs: whether an id is carried, whether a frame
 * decoded, and whether the host lost its device. */
#ifndef AV_OPENCL_BACKEND_HPP
#define AV_OPENCL_BACKEND_HPP

#include "device/executor/cl_event_signals.hpp"
#include "device/executor/cl_handle_registry.hpp"
#include "device/executor/cl_shared_objects.hpp"
#include "device/executor/guest_region.hpp"
#include "device/executor/host_svm_ranges.hpp"
#include "device/executor/session_backend.hpp"
#include "device/executor/gl_context_attributes.hpp"
#include "device/shim/gl_context.hpp"
#include "device/shim/gl_host_import.hpp"
#include "device/shim/gl_shared_buffer.hpp"
#include "proto/generated/opencl/cl_real_dispatch.h"

#include <stdint.h>

namespace av {

class GlDisplay;
class OpenClDevice;
/* What the tests reach the extension-string surgery through: `without` is
 * private because nothing but `on_clGetDeviceInfo` may call it, and its cases
 * are strings no host answers. */
struct OpenClBackendProbe;

class OpenClBackend final : public clproto::RealDispatch, public SessionBackend {
public:
    OpenClBackend();

    /* Binds this context to the platform and device the host found for its
     * GPU, which must outlive it. False for a null device: a GPU the host
     * found no OpenCL for runs nothing, so the context is refused at setup
     * rather than shown an empty platform list.
     *
     * `display` is the same GPU's EGL display, or null. It is what makes
     * sharing possible at all on a host whose OpenCL has no
     * `cl_khr_external_memory`: a GL context on it becomes the vehicle a
     * descriptor is imported through, and the host `cl_context` is created
     * naming that context, so `clCreateFromGLBuffer` can reach the same
     * allocation (feature 053, T019; research.md §21). A null display leaves
     * this importing nothing, which the device answers as a refusal.
     *
     * Whether that works is measured here and not assumed: a host whose
     * OpenCL lists `cl_khr_gl_sharing` can still refuse every context made
     * against a GL context of this one's - which is what happens inside QEMU
     * on the machine this was written on, where the same calls succeed in an
     * ordinary process (research.md §21). A context is made and released to
     * find out, once, before the guest asks. */
    [[nodiscard]] bool initialize(const OpenClDevice *device, const GlDisplay *display);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* This API exports no descriptor (feature 053,
     * contracts/wire_additions.md): only Vulkan does, so a guest asking this
     * one to export is refused by the device with a status rather than handed
     * an object nothing made. Importing is another matter - see below. */
    [[nodiscard]] bool export_object(uint32_t kind, uint64_t wire_handle, uint32_t &handle_kind,
                                     int &fd) override;
    [[nodiscard]] bool import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                     uint64_t &wire_handle) override;
    void               release_object(uint32_t kind, uint64_t wire_handle) override;

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
    /* The host's answer, with `cl_khr_gl_sharing` struck out of the extension
     * string when this context could not make a GL-sharing context. That name
     * is what the guest's ICD reads as "a shared object can be opened into
     * OpenCL here", so leaving it in on a host that refuses the context would
     * have the ICD report an extension whose every use fails (constitution VI;
     * feature 053, T019). Every other query is forwarded untouched. */
    cl_int     on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size,
                                  void *param_value, size_t *param_value_size_ret) override;
    cl_context on_clCreateContextFromType(const cl_context_properties *properties, cl_device_type device_type,
                                          cl_int *errcode_ret) override;
    /* The host's context is created naming this backend's GL context, so a
     * buffer imported through OpenGL can become a `cl_mem` later. The guest
     * never asks for that and never sees it: what it sent is forwarded with
     * the two properties added, and a host that cannot share gets what the
     * guest wrote (feature 053, T019). */
    cl_context on_clCreateContext(const cl_context_properties *properties, cl_uint num_devices,
                                  const cl_device_id *devices, cl_int *errcode_ret) override;

    /* The shared allocations, remembered so an address the guest hands back
     * can be judged; the host's own allocator answers the allocation itself. */
    void *on_clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size,
                        cl_uint alignment) override;
    void  on_clSVMFree(cl_context context, void *svm_pointer) override;

    /* `bytes` at `offset` of the adopted arena, or null for an offset or a
     * length that does not lie inside it. Every offset here is a number the
     * guest wrote, so this is a bounds check and never a dereference. */
    uint8_t *guest_bytes(uint64_t offset, uint64_t bytes) const;

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
    void  *on_clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_map,
                                 cl_map_flags map_flags, size_t offset, size_t size,
                                 cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                 cl_event *event, cl_int *errcode_ret) override;
    cl_int on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj, void *mapped_ptr,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list, cl_event *event) override;

    /* No guest function pointer crosses the wire, so the callback registered
     * here is this backend's own: it signals the device, which wakes the
     * context's worker, and the guest asks what completed. Refused when the
     * device gave this library nothing to signal with - an older device - or
     * when this context is already watching as many events as it can. */
    cl_int on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type) override;

    /* What the device gives the library to call when a host thread finishes
     * work (executor ABI version 5). */
    void bind_signal(void (*signal)(void *context), void *context);

    /* Releases what the guest never did. A context that dies owning a context
     * or a queue must take it with it. */
    void     shut_down();
    uint64_t ran() const;

private:
    friend struct OpenClBackendProbe;

    /* Every OpenCL command id carries this in its top 16 bits. */
    static constexpr uint32_t opencl_api = 2u;
    /* A context property list this device will copy: OpenCL defines a handful
     * and the decoder has already refused every name but one, so a list longer
     * than this is a guest building one to overflow what copies it. Room for
     * the two this device appends and the terminator is inside it. */
    static constexpr uint32_t max_context_properties = 32u;

    /* CL_SUCCESS for a type the bound device is, which is always a GPU;
     * CL_INVALID_DEVICE_TYPE for no type or a bit OpenCL does not define;
     * CL_DEVICE_NOT_FOUND for any other. */
    cl_int answers_type(cl_device_type device_type) const;

    /* The vehicle: a GL context on this GPU's display, the import that turns a
     * descriptor into a memory object on it, and the buffer maker that puts
     * storage OpenCL can name over that memory. Held only while the host has a
     * display for this GPU. */
    [[nodiscard]] bool sharing() const;
    /* Whether this host will really make a context against `vehicle_`: one is
     * made on the bound device and released again. What it answers is the
     * whole of `sharing()`'s truth, since every import goes through such a
     * context. */
    [[nodiscard]] bool shares_with_gl() const;
    /* `name` struck out of the space-separated list in `text`, which is read
     * no further than `bytes`: the words that stay are written back over it,
     * which never reaches past what has been read. Answers the bytes the list
     * needs afterwards, terminator included. */
    size_t             without(char *text, size_t bytes, const char *name) const;
    /* Whether `length` letters at `text` are the whole of `name`. */
    bool               names(const char *text, size_t length, const char *name) const;
    /* The queue an acquire is enqueued on, made on the guest's context at the
     * first import: the acquire is the device's own bookkeeping and no guest
     * queue should carry it. Null when there is no context to make one on. */
    void *acquire_queue();

    const OpenClDevice *bound_;
    const GlDisplay    *display_;
    /* Opened only when a display was given; a context nothing opened holds
     * nothing and closes to nothing. */
    GlContext      vehicle_;
    GlHostImport   import_;
    GlSharedBuffer buffers_;
    ClSharedObjects    shared_;
    /* The first context the guest made, which is the one a shared object is
     * opened into. An OpenCL application makes one; a second is a context this
     * cannot share with, and an open into it is refused rather than answered
     * with a cl_mem of the wrong context. */
    void              *context_;
    void              *queue_;
    ClHandleRegistry handles_;
    HostSvmRanges    svm_;
    HostSvmRanges    mapped_;
    /* The one import a context has: its blob arena. */
    GuestRegion      arena_;
    ClEventSignals   events_;
    uint64_t       ran_;
    bool           lost_;
};

}  // namespace av

#endif  // AV_OPENCL_BACKEND_HPP
