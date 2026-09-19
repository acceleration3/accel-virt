#include "device/executor/opencl_backend.hpp"

#include "core/trace/function_scope.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/cl_event_registration.hpp"
#include "device/executor/imported_blob.hpp"
#include "proto/cl_blob_backing.hpp"
#include "device/shim/opencl_device.hpp"
#include "proto/generated/opencl/cl_command_names.h"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <CL/cl_ext.h>
#include <string.h>

namespace av {

namespace {

/* Every device type bit OpenCL 3.0 defines. CL_DEVICE_TYPE_ALL is all ones,
 * so it is the one value with bits past these that is not a mistake. */
constexpr cl_device_type defined_types = CL_DEVICE_TYPE_DEFAULT | CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_GPU |
                                         CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_CUSTOM;

}  // namespace

/* What the host's OpenCL calls when an event completes, on a thread of its
 * own. An ABI boundary C requires: the callback is a bare function pointer
 * with nowhere to put a `this`, so the slot is the user data and this
 * forwards straight into it, with no logic of its own. */
extern "C" void CL_CALLBACK rvk_cl_event_completed(cl_event, cl_int, void *user_data)
{
    static_cast<ClEventRegistration *>(user_data)->completed();
}

OpenClBackend::OpenClBackend()
    : bound_(nullptr), handles_(), svm_(), events_(), ran_(0u), lost_(false)
{
    AV_SCOPE("OpenClBackend::OpenClBackend");
}

bool OpenClBackend::initialize(const OpenClDevice *device)
{
    AV_SCOPE("OpenClBackend::initialize");
    bound_ = device;
    return bound_ != nullptr;
}

bool OpenClBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("OpenClBackend::knows");
    if (command_id >> 16u != opencl_api) return false;
    /* The generator writes one name per id it carries. An id the profile does
     * not carry - clGetKernelSuggestedLocalWorkSize is the only one reserved
     * today - is a skip, not a fault: a guest using a command this build left
     * out must not have its context stopped for it. */
    return strcmp(clproto::cmd_id_to_string(command_id), "unknown") != 0;
}

bool OpenClBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("OpenClBackend::run");
    if (!clproto::dispatch_command(*this, command_id, payload, reply, arena)) return false;
    ran_++;
    return true;
}

bool OpenClBackend::lost() const
{
    AV_SCOPE("OpenClBackend::lost");
    return lost_;
}

bool OpenClBackend::adopt(ImportedBlob &blob)
{
    AV_SCOPE("OpenClBackend::adopt");
    /* A host pointer, and one at a time: this is the context's blob arena,
     * and a second would leave the first unreachable with its buffers
     * pointing into memory nothing owns.
     *
     * A dma-buf is still not adopted. That would be
     * clCreateBufferWithProperties with an fd, which is a per-platform
     * question this host answers differently on each of its OpenCL platforms.
     * CL_MEM_USE_HOST_PTR needs no extension at all, which is why this half
     * is the one that works everywhere. */
    if (blob.kind() != RVK_BLOB_HOST_POINTER || arena_.length() != 0u) return false;
    arena_.take(static_cast<uint8_t *>(blob.pointer()), blob.size());
    const uint64_t address = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(blob.pointer()));
    blob.record(address, address);
    return true;
}

void OpenClBackend::abandon(ImportedBlob &blob)
{
    AV_SCOPE("OpenClBackend::abandon");
    if (blob.object() == 0u) return;
    arena_.give_back();
    blob.record(0u, 0u);
}

uint8_t *OpenClBackend::guest_bytes(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("OpenClBackend::guest_bytes");
    return arena_.at(offset, bytes);
}

cl_mem OpenClBackend::on_clCreateBufferWithProperties(cl_context context,
                                                      const cl_mem_properties *properties,
                                                      cl_mem_flags flags, size_t size,
                                                      void *host_ptr, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateBufferWithProperties");
    ClBlobBacking backing;
    if (!backing.read(reinterpret_cast<const uint64_t *>(properties))) {
        return ::clCreateBufferWithProperties(context, properties, flags, size, host_ptr,
                                              errcode_ret);
    }
    /* The offset is the guest's, so the arena judges it before it is a
     * pointer. A buffer made over memory that is not the arena's would let
     * the guest choose what the host writes into. */
    uint8_t *const at = arena_.at(backing.offset(), static_cast<uint64_t>(size));
    if (at == nullptr) {
        if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_VALUE;
        return nullptr;
    }
    /* The property is this wire's and not OpenCL's, so it is not passed on;
     * what the host is asked for is the plain call over the guest's memory.
     * CL_MEM_COPY_HOST_PTR alongside would name host_ptr as well, which this
     * call has none of. */
    const cl_mem_flags over_arena =
        (flags & ~static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR)) |
        static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR);
    return ::clCreateBuffer(context, over_arena, size, at, errcode_ret);
}

void *OpenClBackend::handle_for(clproto::HandleKind kind, uint64_t wire) const
{
    AV_SCOPE("OpenClBackend::handle_for");
    return handles_.resolve(kind, wire);
}

void OpenClBackend::made(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::made");
    /* A registry with no room loses the device rather than hand the guest a
     * number this host would refuse when it came back. */
    if (!handles_.remember(kind, object)) lost_ = true;
}

void OpenClBackend::retained(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::retained");
    handles_.retain(kind, object);
}

void OpenClBackend::released(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::released");
    handles_.release(kind, object);
}

cl_int OpenClBackend::on_clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms)
{
    AV_SCOPE("OpenClBackend::on_clGetPlatformIDs");
    if ((platforms != nullptr && num_entries == 0u) || (platforms == nullptr && num_platforms == nullptr))
        return CL_INVALID_VALUE;
    const cl_uint available = bound_ != nullptr ? 1u : 0u;
    if (num_platforms != nullptr) *num_platforms = available;
    if (available == 0u) return CL_PLATFORM_NOT_FOUND_KHR;
    if (platforms != nullptr) platforms[0] = static_cast<cl_platform_id>(bound_->platform());
    return CL_SUCCESS;
}

cl_int OpenClBackend::on_clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries,
                                        cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("OpenClBackend::on_clGetDeviceIDs");
    if ((devices != nullptr && num_entries == 0u) || (devices == nullptr && num_devices == nullptr))
        return CL_INVALID_VALUE;
    if (bound_ == nullptr || (platform != nullptr && platform != bound_->platform())) return CL_INVALID_PLATFORM;
    const cl_int typed = answers_type(device_type);
    if (num_devices != nullptr) *num_devices = typed == CL_SUCCESS ? 1u : 0u;
    if (typed != CL_SUCCESS) return typed;
    if (devices != nullptr) devices[0] = static_cast<cl_device_id>(bound_->device());
    return CL_SUCCESS;
}

cl_context OpenClBackend::on_clCreateContextFromType(const cl_context_properties *properties,
                                                     cl_device_type device_type, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateContextFromType");
    /* The decoder has already refused a CL_CONTEXT_PLATFORM that is not a
     * platform this context was shown, so the properties name the bound one or
     * none, and both mean the bound device. */
    const cl_int refused = bound_ == nullptr ? CL_INVALID_PLATFORM : answers_type(device_type);
    if (refused == CL_SUCCESS) {
        const cl_device_id device = static_cast<cl_device_id>(bound_->device());
        return RealDispatch::on_clCreateContext(properties, 1u, &device, errcode_ret);
    }
    if (errcode_ret != nullptr) *errcode_ret = refused;
    return nullptr;
}

cl_int OpenClBackend::answers_type(cl_device_type device_type) const
{
    AV_SCOPE("OpenClBackend::answers_type");
    if (device_type == 0u || (device_type != CL_DEVICE_TYPE_ALL && (device_type & ~defined_types) != 0u))
        return CL_INVALID_DEVICE_TYPE;
    /* DEFAULT is the implementation's choice, and the only device this
     * implementation has is the bound one. */
    return (device_type & (CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_DEFAULT)) != 0u ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
}

void OpenClBackend::shut_down()
{
    AV_SCOPE("OpenClBackend::shut_down");
    handles_.release_all(*this);
    /* Before the ranges go: a release may free an allocation, and nothing
     * afterwards may resolve one. */
    svm_.forget_all();
    mapped_.forget_all();
    arena_.give_back();
    /* Every event callback still to come goes stale here. The host cannot be
     * told to stop calling them, so being called and doing nothing is what
     * they do from now on. */
    events_.shut_down();
    bound_ = nullptr;
    ran_ = 0u;
    lost_ = false;
}

uint64_t OpenClBackend::ran() const
{
    AV_SCOPE("OpenClBackend::ran");
    return ran_;
}

void *OpenClBackend::svm_for(uint64_t address, uint64_t bytes) const
{
    AV_SCOPE("OpenClBackend::svm_for");
    void *const allocated = svm_.resolve(address, bytes);
    if (allocated != nullptr) return allocated;
    return mapped_.resolve(address, bytes);
}

void *OpenClBackend::on_clSVMAlloc(cl_context context, cl_svm_mem_flags flags, size_t size,
                                   cl_uint alignment)
{
    AV_SCOPE("OpenClBackend::on_clSVMAlloc");
    void *const allocated = ::clSVMAlloc(context, flags, size, alignment);
    if (allocated == nullptr) return nullptr;
    /* An allocation this cannot remember is one whose address could never be
     * judged, so it is given back rather than handed to a guest that would
     * find every use of it refused. */
    if (!svm_.remember(allocated, static_cast<uint64_t>(size))) {
        ::clSVMFree(context, allocated);
        return nullptr;
    }
    return allocated;
}

void OpenClBackend::on_clSVMFree(cl_context context, void *svm_pointer)
{
    AV_SCOPE("OpenClBackend::on_clSVMFree");
    /* Only what this allocated is freed. The decoder has already refused an
     * address in no range, but it resolves an address *inside* one too, and
     * that is not a pointer the host's allocator was given. */
    if (!svm_.forget(svm_pointer)) return;
    ::clSVMFree(context, svm_pointer);
}

void *OpenClBackend::on_clEnqueueMapBuffer(cl_command_queue command_queue, cl_mem buffer,
                                           cl_bool blocking_map, cl_map_flags map_flags,
                                           size_t offset, size_t size,
                                           cl_uint num_events_in_wait_list,
                                           const cl_event *event_wait_list, cl_event *event,
                                           cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueMapBuffer");
    void *const mapped =
        ::clEnqueueMapBuffer(command_queue, buffer, blocking_map, map_flags, offset, size,
                             num_events_in_wait_list, event_wait_list, event, errcode_ret);
    if (mapped == nullptr) return nullptr;
    /* A mapping this cannot remember is one whose unmap could never be
     * judged, and a buffer that can never be unmapped is worse than a map
     * that failed: the guest would hold it for the life of the context. */
    if (!mapped_.remember(mapped, static_cast<uint64_t>(size))) {
        static_cast<void>(::clEnqueueUnmapMemObject(command_queue, buffer, mapped, 0u, nullptr,
                                                    nullptr));
        if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_RESOURCES;
        return nullptr;
    }
    return mapped;
}

cl_int OpenClBackend::on_clEnqueueUnmapMemObject(cl_command_queue command_queue, cl_mem memobj,
                                                 void *mapped_ptr, cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueUnmapMemObject");
    const cl_int unmapped = ::clEnqueueUnmapMemObject(
        command_queue, memobj, mapped_ptr, num_events_in_wait_list, event_wait_list, event);
    /* Forgotten only once the host has taken it: a refused unmap leaves the
     * mapping live, and an address the guest may still name. */
    if (unmapped == CL_SUCCESS) static_cast<void>(mapped_.forget(mapped_ptr));
    return unmapped;
}

cl_int OpenClBackend::on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type)
{
    AV_SCOPE("OpenClBackend::on_clSetEventCallback");
    /* A device that gave this library nothing to signal with cannot wake the
     * guest, and a callback nobody hears is worse than a refusal: the guest
     * would wait on it forever. */
    if (!events_.bound()) return CL_INVALID_OPERATION;
    ClEventRegistration *const slot = events_.take();
    if (slot == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int registered =
        ::clSetEventCallback(event, command_exec_callback_type, &rvk_cl_event_completed, slot);
    if (registered != CL_SUCCESS) events_.give_back(slot);
    return registered;
}

void OpenClBackend::bind_signal(void (*signal)(void *context), void *context)
{
    AV_SCOPE("OpenClBackend::bind_signal");
    events_.bind(signal, context);
}

bool OpenClBackend::export_object(uint32_t, uint64_t, uint32_t &handle_kind, int &fd)
{
    AV_SCOPE("OpenClBackend::export_object");
    handle_kind = 0u;
    fd = -1;
    return false;
}

bool OpenClBackend::import_object(uint32_t, uint32_t, int, uint64_t, uint64_t &wire_handle)
{
    AV_SCOPE("OpenClBackend::import_object");
    wire_handle = 0u;
    return false;
}

void OpenClBackend::release_object(uint32_t, uint64_t)
{
    AV_SCOPE("OpenClBackend::release_object");
}

}  // namespace av
