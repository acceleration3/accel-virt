#include "executor/opencl/opencl_backend.hpp"

#include "executor/opengl/opengl_backend.hpp"

#include <CL/cl_gl.h>

#include "executor/shim/gl_context.hpp"
#include "executor/shim/gl_display.hpp"
#include "executor/shim/gl_host_import.hpp"
#include "executor/shim/gl_shared_buffer.hpp"
#include "executor/shim/opencl_device.hpp"

#include "core/support/untestable.hpp"
#include "core/trace/function_scope.hpp"
#include "device/abi/av_executor.h"
#include "executor/opencl/cl_event_registration.hpp"
#include "executor/imported_blob.hpp"
#include "proto/resources/cl_blob_backing.hpp"
#include "executor/shim/opencl_device.hpp"
#include "proto/generated/opencl/cl_command_names.h"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "proto/generated/opencl/cl_protocol_ids.h"
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

/* What a host says when its OpenCL can name an OpenGL object, which is the
 * only way a shared object reaches OpenCL on the hosts this runs on. */
constexpr const char *gl_sharing = "cl_khr_gl_sharing";

}  // namespace

/* What the host's OpenCL calls when an event completes, on a thread of its
 * own. An ABI boundary C requires: the callback is a bare function pointer
 * with nowhere to put a `this`, so the slot is the user data and this
 * forwards straight into it, with no logic of its own. */
extern "C" void CL_CALLBACK av_cl_event_completed(cl_event, cl_int, void *user_data)
{
    static_cast<ClEventRegistration *>(user_data)->completed();
}

OpenClBackend::OpenClBackend()
    : bound_(nullptr), display_(nullptr), vehicle_(), import_(), buffers_(), shared_(), context_(nullptr),
      queue_(nullptr), handles_(), svm_(), events_(), ran_(0u), lost_(false)
{
    AV_SCOPE("OpenClBackend::OpenClBackend");
}

bool OpenClBackend::initialize(const OpenClDevice *device, const GlDisplay *display, const OpenGlBackend *share)
{
    AV_SCOPE("OpenClBackend::initialize");
    bound_ = device;
    if (bound_ == nullptr) return false;
    if (share != nullptr) return display != nullptr && share_with(*display, *share);
    /* The vehicle is optional and its failure is not this context's: a GPU
     * with no EGL display, or a driver that will not make a context, leaves
     * OpenCL running everything it ran before and refusing to open a shared
     * object (feature 053, T019). */
    if (display == nullptr || !display->open()) return true;
    /* The defaults: this context draws nothing and is never shown to the
     * guest, so it wants no version, no profile and no debug flag. */
    const GlContextAttributes plain(nullptr);
    if (!vehicle_.open(*display, plain, nullptr)) return true;
    display_ = display;
    import_.configure(vehicle_);
    buffers_.configure(vehicle_);
    /* Measured before anything is claimed, and all three together because a
     * context that cannot do every one of them cannot open a shared object at
     * all: it then runs exactly as a context on a GPU with no display does. */
    if (!import_.imports_memory() || !buffers_.makes_buffers() || !shares_with_gl()) { AV_UNTESTABLE("no host beside a test here refuses any of the three; one inside QEMU refuses the last");
        display_ = nullptr;
        vehicle_.close();
        return true;
    }
    return true;
}

/* A vehicle in the guest's GL context's share group, or no context at all. */
bool OpenClBackend::share_with(const GlDisplay &display, const OpenGlBackend &share)
{
    AV_SCOPE("OpenClBackend::share_with");
    const GlContextAttributes plain(nullptr);
    if (!display.open() || !vehicle_.open(display, plain, &share.context())) return false;
    display_ = &display;
    import_.configure(vehicle_);
    buffers_.configure(vehicle_);
    if (shares_with_gl()) return true;
    display_ = nullptr;
    vehicle_.close();
    return false;
}

cl_int OpenClBackend::on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name,
                                         size_t param_value_size, void *param_value,
                                         size_t *param_value_size_ret)
{
    AV_SCOPE("OpenClBackend::on_clGetDeviceInfo");
    const cl_int answered = RealDispatch::on_clGetDeviceInfo(device, param_name, param_value_size,
                                                             param_value, param_value_size_ret);
    if (param_name != CL_DEVICE_EXTENSIONS || answered != CL_SUCCESS || param_value == nullptr ||
        sharing())
        return answered;
    const size_t left = without(static_cast<char *>(param_value), param_value_size, gl_sharing);
    /* A caller asking with no room is told the host's own size, which is never
     * smaller than this: a list may need less room than it was promised, and
     * one that has been read is told what it got. */
    if (param_value_size_ret != nullptr) *param_value_size_ret = left;
    return answered;
}

bool OpenClBackend::names(const char *text, size_t length, const char *name) const
{
    AV_SCOPE("OpenClBackend::names");
    size_t letter = 0u;
    while (letter < length && name[letter] != '\0' && text[letter] == name[letter]) letter++;
    return letter == length && name[letter] == '\0';
}

size_t OpenClBackend::without(char *text, size_t bytes, const char *name) const
{
    AV_SCOPE("OpenClBackend::without");
    size_t length = 0u;
    while (length < bytes && text[length] != '\0') length++;
    size_t written = 0u;
    size_t at = 0u;
    while (at < length) {
        while (at < length && text[at] == ' ') at++;
        size_t end = at;
        while (end < length && text[end] != ' ') end++;
        /* Written back over what has already been read, and never past it: a
         * word that stays moves up by however many were dropped before it. */
        if (end != at && !names(&text[at], end - at, name)) {
            if (written != 0u) text[written++] = ' ';
            for (size_t letter = at; letter < end; letter++) text[written++] = text[letter];
        }
        at = end;
    }
    /* Only where it fits: a list that filled its room with no terminator gets
     * none written past the end of that room. */
    if (written < bytes) text[written] = '\0';
    return written + 1u;
}

bool OpenClBackend::shares_with_gl() const
{
    AV_SCOPE("OpenClBackend::shares_with_gl");
    const cl_context_properties named[] = {
        CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(bound_->platform()),
        CL_GL_CONTEXT_KHR,   reinterpret_cast<cl_context_properties>(vehicle_.handle()),
        CL_EGL_DISPLAY_KHR,  reinterpret_cast<cl_context_properties>(display_->handle()),
        0};
    cl_device_id const one = static_cast<cl_device_id>(bound_->device());
    cl_int             made = CL_SUCCESS;
    cl_context const   asked = ::clCreateContext(named, 1u, &one, nullptr, nullptr, &made);
    if (asked == nullptr) { AV_UNTESTABLE("no host beside a test here refuses a GL-sharing context"); return false; }
    static_cast<void>(::clReleaseContext(asked));
    return true;
}

void *OpenClBackend::acquire_queue()
{
    AV_SCOPE("OpenClBackend::acquire_queue");
    if (queue_ != nullptr) return queue_;
    /* A context and a device, because `import_object` is the only caller and
     * it has already refused a backend without either. */
    cl_int made = CL_SUCCESS;
    queue_ = ::clCreateCommandQueueWithProperties(static_cast<cl_context>(context_),
                                                  static_cast<cl_device_id>(bound_->device()), nullptr,
                                                  &made);
    return queue_;
}

bool OpenClBackend::sharing() const
{
    AV_SCOPE("OpenClBackend::sharing");
    return display_ != nullptr;
}

cl_context OpenClBackend::on_clCreateContext(const cl_context_properties *properties, cl_uint num_devices,
                                             const cl_device_id *devices, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateContext");
    if (!sharing()) return RealDispatch::on_clCreateContext(properties, num_devices, devices, errcode_ret);
    /* The guest's list, with the two properties that let this context name a
     * GL object appended before its terminator. The decoder has already
     * refused anything but a CL_CONTEXT_PLATFORM naming the bound platform,
     * so what is copied is short and known. */
    cl_context_properties named[max_context_properties] = {};
    uint32_t               at = 0u;
    if (properties != nullptr) {
        while (properties[at] != 0 && at + 4u < max_context_properties) {
            named[at] = properties[at];
            named[at + 1u] = properties[at + 1u];
            at += 2u;
        }
        /* A list longer than this is one the guest built to overflow it. */
        if (properties[at] != 0) {
            if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_PROPERTY;
            return nullptr;
        }
    }
    named[at] = CL_GL_CONTEXT_KHR;
    named[at + 1u] = reinterpret_cast<cl_context_properties>(vehicle_.handle());
    named[at + 2u] = CL_EGL_DISPLAY_KHR;
    named[at + 3u] = reinterpret_cast<cl_context_properties>(display_->handle());
    named[at + 4u] = 0;
    /* Whatever it answers is the guest's answer: a context that got this far
     * was made on a host that made one at bind time, so a refusal here is a
     * real one and not this driver's own arrangement failing. */
    cl_int           made = CL_SUCCESS;
    cl_context const shared = RealDispatch::on_clCreateContext(named, num_devices, devices, &made);
    if (errcode_ret != nullptr) *errcode_ret = made;
    return shared;
}

bool OpenClBackend::knows(uint32_t command_id) const
{
    AV_SCOPE("OpenClBackend::knows");
    /* An id the profile does not carry - clGetKernelSuggestedLocalWorkSize is
     * the only one reserved today - is a skip, not a fault: a guest using a
     * command this build left out must not have its context stopped for it. */
    return clproto::knows_command(command_id);
}

bool OpenClBackend::run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                        WireArena &arena)
{
    AV_SCOPE("OpenClBackend::run");
    /* Anything but another read or a question about an object may change a
     * buffer, or free one whose handle a new one then takes. */
    if (command_id != clproto::kCmd_clEnqueueReadBuffer &&
        strncmp(clproto::cmd_id_to_string(command_id), "clGet", 5u) != 0)
        read_ahead_.forget();
    if (!clproto::dispatch_command(*this, command_id, payload, reply, arena)) return false;
    ran_++;
    return true;
}

bool OpenClBackend::poll()
{
    AV_SCOPE("OpenClBackend::poll");
    return false;
}

void OpenClBackend::gpu_progress(uint64_t &marked, uint64_t &reached) const
{
    AV_SCOPE("OpenClBackend::gpu_progress");
    marked = 0u;
    reached = 0u;
}

void OpenClBackend::end_batch()
{
    AV_SCOPE("OpenClBackend::end_batch");
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
    return arena_.adopt_pointer(blob);
}

void OpenClBackend::abandon(ImportedBlob &blob)
{
    AV_SCOPE("OpenClBackend::abandon");
    arena_.abandon(blob);
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
    /* The first context is the one a shared object is opened into: it is the
     * one created naming this backend's GL context, so it is the only one
     * clCreateFromGLBuffer would take (feature 053, T019). */
    if (kind == clproto::HandleKind::context && context_ == nullptr) context_ = object;
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
    /* The shared objects before the handles, because each holds a cl_mem the
     * registry would otherwise release underneath it - and the registry lets
     * go of each first, so release_all does not release it again - and its
     * GL objects go with it while the vehicle context is still there to
     * delete them. */
    uint64_t mem = 0u;
    uint32_t buffer = 0u;
    uint32_t memory = 0u;
    while (shared_.take_any(mem, buffer, memory)) {
        cl_mem const shared = reinterpret_cast<cl_mem>(mem);
        handles_.release(clproto::HandleKind::mem, shared);
        static_cast<void>(::clEnqueueReleaseGLObjects(static_cast<cl_command_queue>(queue_), 1u, &shared, 0u,
                                                      nullptr, nullptr));
        static_cast<void>(::clFinish(static_cast<cl_command_queue>(queue_)));
        static_cast<void>(::clReleaseMemObject(shared));
        buffers_.destroy(buffer);
        import_.delete_memory(memory);
    }
    handles_.release_all(*this);
    /* Before the ranges go: a release may free an allocation, and nothing
     * afterwards may resolve one. */
    svm_.forget_all();
    mapped_.forget_all();
    arena_.give_back();
    if (queue_ != nullptr) {
        static_cast<void>(::clReleaseCommandQueue(static_cast<cl_command_queue>(queue_)));
        queue_ = nullptr;
    }
    context_ = nullptr;
    /* Every event callback still to come goes stale here. The host cannot be
     * told to stop calling them, so being called and doing nothing is what
     * they do from now on. */
    events_.shut_down();
    /* The vehicle last: its context is what deleted the GL objects above. */
    buffers_.give_back();
    import_.give_back();
    vehicle_.close();
    display_ = nullptr;
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
    if (!svm_.remember(allocated, static_cast<uint64_t>(size), 0u)) {
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

cl_int OpenClBackend::on_clReleaseMemObject(cl_mem memobj)
{
    AV_SCOPE("OpenClBackend::on_clReleaseMemObject");
    static_cast<void>(mapped_.forget_owned(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(memobj))));
    return RealDispatch::on_clReleaseMemObject(memobj);
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
    if (!mapped_.remember(mapped, static_cast<uint64_t>(size), static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buffer)))) {
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

cl_int OpenClBackend::on_clEnqueueReadBuffer(cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read,
                                             size_t offset, size_t size, void *ptr, cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueReadBuffer");
    cl_int served = CL_SUCCESS;
    if (blocking_read == CL_TRUE && num_events_in_wait_list == 0u && event == nullptr &&
        read_ahead_.read(command_queue, buffer, offset, size, ptr, served))
        return served;
    return RealDispatch::on_clEnqueueReadBuffer(command_queue, buffer, blocking_read, offset, size, ptr,
                                                num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clEnqueueWriteBuffer(cl_command_queue command_queue, cl_mem buffer,
                                              cl_bool blocking_write, size_t offset, size_t size,
                                              const void *ptr, cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueWriteBuffer");
    static_cast<void>(blocking_write);
    return RealDispatch::on_clEnqueueWriteBuffer(command_queue, buffer, CL_TRUE, offset, size, ptr,
                                                 num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clEnqueueWriteImage(cl_command_queue command_queue, cl_mem image,
                                             cl_bool blocking_write, const size_t *origin,
                                             const size_t *region, size_t input_row_pitch,
                                             size_t input_slice_pitch, const void *ptr,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueWriteImage");
    static_cast<void>(blocking_write);
    return RealDispatch::on_clEnqueueWriteImage(command_queue, image, CL_TRUE, origin, region,
                                                input_row_pitch, input_slice_pitch, ptr,
                                                num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clEnqueueWriteBufferRect(
    cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_write, const size_t *buffer_origin,
    const size_t *host_origin, const size_t *region, size_t buffer_row_pitch, size_t buffer_slice_pitch,
    size_t host_row_pitch, size_t host_slice_pitch, const void *ptr, cl_uint num_events_in_wait_list,
    const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueWriteBufferRect");
    static_cast<void>(blocking_write);
    return RealDispatch::on_clEnqueueWriteBufferRect(
        command_queue, buffer, CL_TRUE, buffer_origin, host_origin, region, buffer_row_pitch,
        buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list,
        event_wait_list, event);
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
        ::clSetEventCallback(event, command_exec_callback_type, &av_cl_event_completed, slot);
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

bool OpenClBackend::import_object(uint32_t kind, uint32_t handle_kind, int fd, uint64_t size,
                                  uint64_t &wire_handle)
{
    AV_SCOPE("OpenClBackend::import_object");
    wire_handle = 0u;
    /* Memory only, and only as a dma-buf: a fence is a host semaphore that
     * OpenCL names through a different extension this host has not got, and an
     * opaque fd is another driver's private handle that OpenGL cannot read
     * (research.md §7). */
    if (kind != AV_SHARED_MEMORY || handle_kind != AV_SHARED_HANDLE_DMA_BUF || fd < 0 || size == 0u)
        return false;
    if (!sharing() || context_ == nullptr) return false;
    if (acquire_queue() == nullptr) { AV_UNTESTABLE("no host beside a test here refuses a command queue"); return false; }
    uint32_t memory = 0u;
    if (!import_.import_memory(fd, size, memory)) return false; // AV_UNTESTABLE("a descriptor this host's GL refuses; an ordinary fd is taken and then faults in the driver")
    uint32_t buffer = 0u;
    if (!buffers_.make(memory, size, buffer)) { AV_UNTESTABLE("no host beside a test here refuses storage over memory it imported");
        import_.delete_memory(memory);
        return false;
    }
    cl_int   made = CL_SUCCESS;
    cl_mem const shared =
        ::clCreateFromGLBuffer(static_cast<cl_context>(context_), CL_MEM_READ_WRITE, buffer, &made);
    if (shared == nullptr) { AV_UNTESTABLE("no host beside a test here refuses a buffer it imported itself");
        buffers_.destroy(buffer);
        import_.delete_memory(memory);
        return false;
    }
    /* Acquired once and held: the acquire is the rule about OpenGL and OpenCL
     * taking turns over one object, and the GL context here is an import
     * vehicle that never draws - so OpenCL's turn is the whole of the object's
     * life, and no enqueue has to be bracketed. Proven on this host: a write
     * and a read across enqueues under one acquire, and what OpenCL wrote read
     * back through the exporting Vulkan device's own mapping. */
    const bool taken = ::clEnqueueAcquireGLObjects(static_cast<cl_command_queue>(queue_), 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS && ::clFinish(static_cast<cl_command_queue>(queue_)) == CL_SUCCESS;  // AV_UNTESTABLE("no host beside a test here refuses an acquire")
    /* Named in the guest's commands from now on, as any cl_mem the host made
     * is: one the registry did not hold would resolve to nothing in every
     * enqueue that names it. */
    if (!taken || !handles_.remember(clproto::HandleKind::mem, shared) || !shared_.remember(reinterpret_cast<uint64_t>(shared), buffer, memory)) { AV_UNTESTABLE("the acquire above, and tables no guest fills without thousands of live objects");
        handles_.release(clproto::HandleKind::mem, shared);
        static_cast<void>(::clReleaseMemObject(shared));
        buffers_.destroy(buffer);
        import_.delete_memory(memory);
        return false;
    }
    wire_handle = reinterpret_cast<uint64_t>(shared);
    return true;
}

void OpenClBackend::release_object(uint32_t, uint64_t wire_handle)
{
    AV_SCOPE("OpenClBackend::release_object");
    uint32_t buffer = 0u;
    uint32_t memory = 0u;
    if (!shared_.forget(wire_handle, buffer, memory)) return;
    /* The reverse of the making: the turn given back, then the cl_mem, then
     * the buffer that was its storage, and the memory object last. */
    /* The queue is the import's own and outlives every row it made, so a row
     * to give back is a queue to give it back on. */
    cl_mem const shared = reinterpret_cast<cl_mem>(wire_handle);
    handles_.release(clproto::HandleKind::mem, shared);
    static_cast<void>(::clEnqueueReleaseGLObjects(static_cast<cl_command_queue>(queue_), 1u, &shared, 0u,
                                                  nullptr, nullptr));
    static_cast<void>(::clFinish(static_cast<cl_command_queue>(queue_)));
    static_cast<void>(::clReleaseMemObject(shared));
    buffers_.destroy(buffer);
    import_.delete_memory(memory);
}

}  // namespace av
