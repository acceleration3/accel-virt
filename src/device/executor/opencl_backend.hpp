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
#include "device/executor/guest_region.hpp"
#include "device/executor/host_svm_ranges.hpp"
#include "device/executor/session_backend.hpp"
#include "proto/generated/opencl/cl_real_dispatch.h"

#include <stdint.h>

namespace av {

class OpenClDevice;

class OpenClBackend final : public clproto::RealDispatch, public SessionBackend {
public:
    OpenClBackend();

    /* Binds this context to the platform and device the host found for its
     * GPU, which must outlive it. False for a null device: a GPU the host
     * found no OpenCL for runs nothing, so the context is refused at setup
     * rather than shown an empty platform list. */
    [[nodiscard]] bool initialize(const OpenClDevice *device);

    bool knows(uint32_t command_id) const override;
    [[nodiscard]] bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                           WireArena &arena) override;
    bool lost() const override;
    [[nodiscard]] bool adopt(ImportedBlob &blob) override;
    void abandon(ImportedBlob &blob) override;

    /* This API exports no descriptor and imports none (feature 053,
     * contracts/wire_additions.md): only Vulkan does, so a guest sharing
     * through this API is refused by the device with a status rather than
     * handed an object nothing made. */
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
    cl_context on_clCreateContextFromType(const cl_context_properties *properties, cl_device_type device_type,
                                          cl_int *errcode_ret) override;

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
    /* Every OpenCL command id carries this in its top 16 bits. */
    static constexpr uint32_t opencl_api = 2u;

    /* CL_SUCCESS for a type the bound device is, which is always a GPU;
     * CL_INVALID_DEVICE_TYPE for no type or a bit OpenCL does not define;
     * CL_DEVICE_NOT_FOUND for any other. */
    cl_int answers_type(cl_device_type device_type) const;

    const OpenClDevice *bound_;
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
