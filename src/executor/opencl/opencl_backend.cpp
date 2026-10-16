#include "executor/opencl/opencl_backend.hpp"

#include "core/support/extension_words.hpp"
#include "core/support/growable_array.hpp"

#include "device/host/stdio_log.hpp"
#include "core/log/driver_log.hpp"
#include "executor/sharing/shared_image_shape.hpp"

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
#include "executor/import_table.hpp"
#include "executor/source_memory.hpp"
#include "proto/d3d/dxgi_format.hpp"
#include "proto/resources/cl_blob_backing.hpp"
#include "executor/shim/opencl_device.hpp"
#include "proto/generated/opencl/cl_command_names.h"
#include "proto/generated/opencl/cl_dispatch_server.h"
#include "proto/generated/opencl/cl_protocol_ids.h"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

#include <CL/cl_ext.h>
#include <string.h>
#include <time.h>

namespace av {

namespace {

/* Every device type bit OpenCL 3.0 defines. CL_DEVICE_TYPE_ALL is all ones,
 * so it is the one value with bits past these that is not a mistake. */
constexpr cl_device_type defined_types = CL_DEVICE_TYPE_DEFAULT | CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_GPU |
                                         CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_CUSTOM;

/* The two extension names the guest is shown as this executor finds them
 * (on_clGetDeviceInfo). */
constexpr const char *gl_sharing = "cl_khr_gl_sharing";
constexpr const char *external_memory = "cl_khr_external_memory";

/* Dependents before what they depend on, the order an application tearing
 * down in reverse would use, so the driver is never asked to free a context
 * it still has queues and buffers on. */
constexpr clproto::HandleKind release_order[] = {
    clproto::HandleKind::event,   clproto::HandleKind::kernel,        clproto::HandleKind::program,
    clproto::HandleKind::sampler, clproto::HandleKind::mem,           clproto::HandleKind::command_queue,
    clproto::HandleKind::context, clproto::HandleKind::device};

uint64_t number_of(const void *object)
{
    AV_SCOPE("number_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(object));
}

}  // namespace

/* What the host's OpenCL calls when an event completes, on a thread of its
 * own. An ABI boundary C requires: the callback is a bare function pointer
 * with nowhere to put a `this`, so the slot is the user data and this
 * forwards straight into it, with no logic of its own. */
extern "C" void CL_CALLBACK av_cl_event_completed(cl_event, cl_int, void *user_data)
{
    static_cast<ClEventRegistration *>(user_data)->completed();
}

OpenClBackend::OpenClBackend() : OpenClBackend(heap_)
{
    AV_SCOPE("OpenClBackend::OpenClBackend");
}

/* heap_ is constructed before blocks_ is bound to it, whichever constructor
 * runs: it is declared first. */
OpenClBackend::OpenClBackend(BlockSource &blocks)
    : heap_(), blocks_(blocks), bound_(nullptr), envelope_(), arguments_(), binaries_(), il_(), argument_kinds_(),
      vehicle_(), import_(), buffers_(), renderbuffer_textures_(), object_bytes_(),
      direct_(blocks, vehicle_, renderbuffer_textures_), copied_(blocks, object_bytes_, renderbuffer_textures_),
      gl_(nullptr), reaches_gl_(false), imports_(false), context_(nullptr), queue_(nullptr), handles_(blocks),
      svm_(blocks), mapped_(blocks), imported_(nullptr), events_(blocks), pacer_(), paced_queue_(nullptr), marker_(nullptr), ran_(0u),
      lost_(false)
{
    AV_SCOPE("OpenClBackend::OpenClBackend");
}

bool OpenClBackend::initialize(const OpenClDevice *device, const GlDisplay *display, const OpenGlBackend *share,
                               const ImportTable &imports)
{
    AV_SCOPE("OpenClBackend::initialize");
    imported_ = &imports;
    bound_ = device;
    if (bound_ == nullptr) return false;
    /* A device that will not describe itself seals no binary and opens
     * none; it runs everything else as before. */
    static_cast<void>(envelope_.identify(static_cast<cl_platform_id>(bound_->platform()),
                                         static_cast<cl_device_id>(bound_->device())));
    /* The vehicle is optional and its failure is not a plain context's: a GPU
     * with no EGL display, or a driver that will not make a context, leaves
     * OpenCL running everything it ran before and sharing nothing (feature
     * 053, T019). A context made from a GL context asked for nothing else.
     * The defaults: the vehicle draws nothing and is never shown to the
     * guest, so it wants no version, no profile and no debug flag. */
    const GlContextAttributes plain(nullptr);
    if (display == nullptr || !display->open() ||
        !vehicle_.open(*display, plain, share != nullptr ? &share->context() : nullptr))
        return share == nullptr;
    import_.configure(vehicle_);
    buffers_.configure(vehicle_, import_);
    renderbuffer_textures_.configure(vehicle_, *display);
    object_bytes_.configure(vehicle_);
    direct_.configure(*display);
    if (shares_with_gl(*display)) {
        gl_ = &direct_;
        imports_ = buffers_.makes_buffers();
        reaches_gl_ = true;
    } else {
        reaches_gl_ = copy_gl_objects();
    }
    /* A plain context keeps the vehicle only to import through. */
    if (!reaches_gl_ || (share == nullptr && !imports_)) close_vehicle();
    return reaches_gl_ || share == nullptr;
}

bool OpenClBackend::copy_gl_objects()
{
    AV_SCOPE("OpenClBackend::copy_gl_objects");
    if (!object_bytes_.moves_bytes()) return false;
    gl_ = &copied_;
    imports_ = false;
    return true;
}

void OpenClBackend::close_vehicle()
{
    AV_SCOPE("OpenClBackend::close_vehicle");
    if (gl_ != nullptr) gl_->forget_all();
    gl_ = nullptr;
    imports_ = false;
    object_bytes_.give_back();
    renderbuffer_textures_.give_back();
    buffers_.give_back();
    import_.give_back();
    vehicle_.close();
}

cl_int OpenClBackend::on_clGetDeviceInfo(cl_device_id device, cl_device_info param_name,
                                         size_t param_value_size, void *param_value,
                                         size_t *param_value_size_ret)
{
    AV_SCOPE("OpenClBackend::on_clGetDeviceInfo");
    if (param_name != CL_DEVICE_EXTENSIONS)
        return RealDispatch::on_clGetDeviceInfo(device, param_name, param_value_size, param_value,
                                                param_value_size_ret);
    size_t       host_bytes = 0u;
    const cl_int sized = RealDispatch::on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0u, nullptr, &host_bytes);
    if (sized != CL_SUCCESS) return sized;
    GrowableArray<char> host(blocks_, Pool::paged);
    if (host_bytes > GrowableArray<char>::most_values ||
        host.resize(static_cast<uint32_t>(host_bytes), '\0') != Status::ok)
        return CL_OUT_OF_HOST_MEMORY;
    const cl_int asked =
        RealDispatch::on_clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, host_bytes, host.begin(), nullptr);
    if (asked != CL_SUCCESS) return asked;
    ExtensionWords shown(blocks_, Pool::paged);
    if (shown.hold(host.begin(), host_bytes) != Status::ok) return CL_OUT_OF_HOST_MEMORY;
    if (!reaches_gl_) shown.strike(gl_sharing);
    if (!imports_) shown.strike(external_memory);
    else if (shown.add(external_memory) != Status::ok) return CL_OUT_OF_HOST_MEMORY;
    const size_t needed = shown.length() + 1u;
    if (param_value_size_ret != nullptr) *param_value_size_ret = needed;
    if (param_value == nullptr) return CL_SUCCESS;
    if (param_value_size < needed) return CL_INVALID_VALUE;
    __builtin_memcpy(param_value, shown.text(), needed);
    return CL_SUCCESS;
}

cl_int OpenClBackend::on_clGetProgramInfo(cl_program program, cl_program_info param_name, size_t param_value_size,
                                          void *param_value, size_t *param_value_size_ret)
{
    AV_SCOPE("OpenClBackend::on_clGetProgramInfo");
    if (param_name == CL_PROGRAM_BINARIES)
        return sealed_binaries(program, param_value_size, param_value, param_value_size_ret);
    if (param_name != CL_PROGRAM_BINARY_SIZES)
        return RealDispatch::on_clGetProgramInfo(program, param_name, param_value_size, param_value,
                                                 param_value_size_ret);
    size_t        sizes[max_program_devices] = {};
    const cl_uint devices = program_binary_sizes(program, sizes);
    if (devices == 0u) return CL_INVALID_PROGRAM;
    for (cl_uint index = 0u; index < devices; ++index) sizes[index] = envelope_.sealed_bytes(sizes[index]);
    const size_t answer = devices * sizeof(size_t);
    if (param_value_size_ret != nullptr) *param_value_size_ret = answer;
    if (param_value == nullptr) return CL_SUCCESS;
    if (param_value_size < answer) return CL_INVALID_VALUE;
    __builtin_memcpy(param_value, sizes, answer);
    return CL_SUCCESS;
}

cl_uint OpenClBackend::program_binary_sizes(cl_program program, size_t *sizes)
{
    AV_SCOPE("OpenClBackend::program_binary_sizes");
    cl_uint devices = 0u;
    if (::clGetProgramInfo(program, CL_PROGRAM_NUM_DEVICES, sizeof devices, &devices, nullptr) != CL_SUCCESS ||
        devices == 0u || devices > max_program_devices)
        return 0u;
    if (::clGetProgramInfo(program, CL_PROGRAM_BINARY_SIZES, devices * sizeof(size_t), sizes, nullptr) != CL_SUCCESS)
        return 0u;
    /* A device that will not describe itself has nothing it can seal, and
     * an answer of no binary is the API's own for that. */
    if (!envelope_.identified())
        for (cl_uint index = 0u; index < devices; ++index) sizes[index] = 0u;
    return devices;
}

cl_int OpenClBackend::sealed_binaries(cl_program program, size_t param_value_size, void *param_value,
                                      size_t *param_value_size_ret)
{
    AV_SCOPE("OpenClBackend::sealed_binaries");
    size_t        sizes[max_program_devices] = {};
    const cl_uint devices = program_binary_sizes(program, sizes);
    if (devices == 0u) return CL_INVALID_PROGRAM;
    size_t host_total = 0u;
    size_t sealed_total = 0u;
    for (cl_uint index = 0u; index < devices; ++index) {
        if (__builtin_add_overflow(host_total, sizes[index], &host_total) ||
            __builtin_add_overflow(sealed_total, envelope_.sealed_bytes(sizes[index]), &sealed_total))
            return CL_OUT_OF_RESOURCES;
    }
    if (param_value_size_ret != nullptr) *param_value_size_ret = sealed_total;
    if (param_value == nullptr || sealed_total == 0u) return CL_SUCCESS;
    if (param_value_size < sealed_total) return CL_INVALID_VALUE;
    if (!binaries_.reserve(host_total)) return CL_OUT_OF_HOST_MEMORY;
    unsigned char *host[max_program_devices] = {};
    size_t         at = 0u;
    for (cl_uint index = 0u; index < devices; ++index) {
        host[index] = sizes[index] != 0u ? binaries_.bytes() + at : nullptr;
        at += sizes[index];
    }
    const cl_int fetched = ::clGetProgramInfo(program, CL_PROGRAM_BINARIES, devices * sizeof host[0], host, nullptr);
    if (fetched != CL_SUCCESS) return fetched;
    uint8_t *into = static_cast<uint8_t *>(param_value);
    for (cl_uint index = 0u; index < devices; ++index) {
        envelope_.seal(host[index], sizes[index], into);
        into += envelope_.sealed_bytes(sizes[index]);
    }
    return CL_SUCCESS;
}

cl_program OpenClBackend::on_clCreateProgramWithBinary(cl_context context, cl_uint num_devices,
                                                       const cl_device_id *device_list, const size_t *lengths,
                                                       const unsigned char **binaries, cl_int *binary_status,
                                                       cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateProgramWithBinary");
    cl_int refusal = CL_SUCCESS;
    if (num_devices == 0u || device_list == nullptr || lengths == nullptr || binaries == nullptr)
        refusal = CL_INVALID_VALUE;
    else if (num_devices != 1u || bound_ == nullptr || device_list[0] != bound_->device())
        refusal = CL_INVALID_DEVICE;
    const uint8_t *host = nullptr;
    size_t         host_bytes = 0u;
    if (refusal == CL_SUCCESS &&
        (!envelope_.open(binaries[0], lengths[0], host, host_bytes) || !arguments_.kept(context, device_list[0])))
        refusal = CL_INVALID_BINARY;
    if (refusal != CL_SUCCESS) {
        if (binary_status != nullptr && refusal == CL_INVALID_BINARY) *binary_status = CL_INVALID_BINARY;
        if (errcode_ret != nullptr) *errcode_ret = refusal;
        return nullptr;
    }
    const unsigned char *opened = host;
    cl_int               status = CL_SUCCESS;
    cl_program const     made =
        RealDispatch::on_clCreateProgramWithBinary(context, 1u, device_list, &host_bytes, &opened, &status, errcode_ret);
    if (binary_status != nullptr) *binary_status = status;
    return made;
}

cl_int OpenClBackend::on_clSetKernelExecInfo(cl_kernel kernel, cl_kernel_exec_info param_name, size_t param_value_size,
                                             const void *param_value)
{
    AV_SCOPE("OpenClBackend::on_clSetKernelExecInfo");
    if (param_name == CL_KERNEL_EXEC_INFO_SVM_FINE_GRAIN_SYSTEM) {
        cl_bool wanted = CL_FALSE;
        if (param_value == nullptr || param_value_size != sizeof wanted) return CL_INVALID_VALUE;
        __builtin_memcpy(&wanted, param_value, sizeof wanted);
        if (wanted != CL_FALSE) return CL_INVALID_OPERATION;
    } else if (param_name != CL_KERNEL_EXEC_INFO_SVM_PTRS) {
        return CL_INVALID_VALUE;
    }
    return RealDispatch::on_clSetKernelExecInfo(kernel, param_name, param_value_size, param_value);
}

bool OpenClBackend::declared_argument(cl_kernel kernel, cl_uint index, clproto::KernelArgument &argument)
{
    AV_SCOPE("OpenClBackend::declared_argument");
    cl_program program = nullptr;
    size_t     length = 0u;
    if (::clGetKernelInfo(kernel, CL_KERNEL_PROGRAM, sizeof program, &program, nullptr) != CL_SUCCESS ||
        ::clGetKernelInfo(kernel, CL_KERNEL_FUNCTION_NAME, 0u, nullptr, &length) != CL_SUCCESS || length == 0u ||
        length > GrowableArray<char>::most_values)
        return false;
    /* As long as the kernel's name is, whatever the application called it. */
    GrowableArray<char> name(blocks_, Pool::paged);
    if (name.resize(static_cast<uint32_t>(length), '\0') != Status::ok ||
        ::clGetKernelInfo(kernel, CL_KERNEL_FUNCTION_NAME, length, name.begin(), nullptr) != CL_SUCCESS)
        return false;
    size_t                count = 0u;
    const uint32_t *const module = il_.words(program, count);
    return module != nullptr && argument_kinds_.kind(module, count, name.begin(), index, argument);
}

bool OpenClBackend::shares_with_gl(const GlDisplay &display) const
{
    AV_SCOPE("OpenClBackend::shares_with_gl");
    const cl_context_properties named[] = {
        CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(bound_->platform()),
        CL_GL_CONTEXT_KHR,   reinterpret_cast<cl_context_properties>(vehicle_.handle()),
        CL_EGL_DISPLAY_KHR,  reinterpret_cast<cl_context_properties>(display.handle()),
        0};
    cl_device_id const one = static_cast<cl_device_id>(bound_->device());
    cl_int             made = CL_SUCCESS;
    cl_context const   asked = ::clCreateContext(named, 1u, &one, nullptr, nullptr, &made);
    /* NVIDIA's OpenCL refuses every EGL context here, which is the copy path. */
    if (asked == nullptr) { AV_UNTESTABLE("no host beside a test here refuses a GL-sharing context; OpenClBackendProbe takes the copy path on one that shares"); return false; }
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

cl_context OpenClBackend::on_clCreateContext(const cl_context_properties *properties, cl_uint num_devices,
                                             const cl_device_id *devices, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateContext");
    if (gl_ == nullptr) return RealDispatch::on_clCreateContext(properties, num_devices, devices, errcode_ret);
    return gl_->make_context(properties, num_devices, devices, errcode_ret);
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

bool OpenClBackend::lose_device()
{
    AV_SCOPE("OpenClBackend::lose_device");
    return false;
}

bool OpenClBackend::lost() const
{
    AV_SCOPE("OpenClBackend::lost");
    return lost_;
}

/* Buffers over either are made over the source's own mapping with
 * CL_MEM_USE_HOST_PTR, which needs no extension on any platform; memory with
 * no mapping is refused, and the guest copies. */
bool OpenClBackend::adopt(const SourceMemory &memory, uint64_t &handle)
{
    AV_SCOPE("OpenClBackend::adopt");
    if (arena_.memory() == nullptr) return arena_.adopt(memory, handle);
    handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&memory));
    return memory.pointer() != nullptr;
}

/* Lent pages go with their import: a buffer still made over them was retired
 * by the guest first (docs/host-memory.md). */
void OpenClBackend::abandon(const SourceMemory &memory, uint64_t)
{
    AV_SCOPE("OpenClBackend::abandon");
    arena_.abandon(memory);
}

void *OpenClBackend::lent_for(uint64_t offset, uint64_t bytes) const
{
    AV_SCOPE("OpenClBackend::lent_for");
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
    /* The offset is the guest's, so the arena - or the import it names -
     * judges it before it is a pointer. A buffer made over memory that is not
     * the guest's would let the guest choose what the host writes into. */
    const SourceMemory *const lent =
        backing.memory() != 0u && imported_ != nullptr ? imported_->named(backing.memory()) : nullptr;
    uint8_t *const at = backing.memory() == 0u ? arena_.at(backing.offset(), static_cast<uint64_t>(size))
                        : lent != nullptr      ? lent->at(backing.offset(), static_cast<uint64_t>(size))
                                               : nullptr;
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
    void *const *const held = handles_.find(wire, kind);
    return held != nullptr ? *held : nullptr;
}

bool OpenClBackend::room_for(uint64_t objects)
{
    AV_SCOPE("OpenClBackend::room_for");
    return handles_.make_room(objects) == Status::ok;
}

void OpenClBackend::made(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::made");
    /* The decoder made room_for every object a reply carries before the ICD
     * made any, so only an ICD answering an address this context still holds
     * as another kind is refused here: the host could no longer say which
     * object the guest's number means, and the device is lost rather than
     * guess. An object remembered nowhere is refused when the guest names it. */
    if (hold(kind, object) == Status::conflict) lost_ = true;
    /* The first context is the one a shared object is opened into: it is the
     * one created naming this backend's GL context, so it is the only one
     * clCreateFromGLBuffer would take (feature 053, T019). */
    if (kind == clproto::HandleKind::context && context_ == nullptr) context_ = object;
}

void OpenClBackend::retained(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::retained");
    handles_.retain(number_of(object), kind);
}

void OpenClBackend::released(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::released");
    const bool last = handles_.release(number_of(object), kind);
    /* What was made for a GL object's cl_mem goes with the guest's last
     * reference: the host's OpenCL holds what it imported itself. */
    if (last && kind == clproto::HandleKind::mem && gl_ != nullptr) gl_->forget(static_cast<cl_mem>(object));
}

Status OpenClBackend::hold(clproto::HandleKind kind, void *object)
{
    AV_SCOPE("OpenClBackend::hold");
    if (object == nullptr) return Status::ok;
    clproto::HandleKind held = kind;
    if (handles_.find_any(number_of(object), held) != nullptr) return held == kind ? Status::ok : Status::conflict;
    return handles_.remember(number_of(object), kind, 0u, object);
}

/* What the ICD answers changes nothing: the context is gone either way, and
 * there is nobody left to tell. */
void OpenClBackend::let_go(clproto::HandleKind kind, void *object, uint32_t times)
{
    AV_SCOPE("OpenClBackend::let_go");
    for (uint32_t n = 0u; n < times; n++) {
        switch (kind) {
        case clproto::HandleKind::event: static_cast<void>(on_clReleaseEvent(static_cast<cl_event>(object))); break;
        case clproto::HandleKind::kernel: static_cast<void>(on_clReleaseKernel(static_cast<cl_kernel>(object))); break;
        case clproto::HandleKind::program: static_cast<void>(on_clReleaseProgram(static_cast<cl_program>(object))); break;
        case clproto::HandleKind::sampler: static_cast<void>(on_clReleaseSampler(static_cast<cl_sampler>(object))); break;
        case clproto::HandleKind::mem: static_cast<void>(on_clReleaseMemObject(static_cast<cl_mem>(object))); break;
        case clproto::HandleKind::command_queue:
            static_cast<void>(on_clReleaseCommandQueue(static_cast<cl_command_queue>(object)));
            break;
        case clproto::HandleKind::context: static_cast<void>(on_clReleaseContext(static_cast<cl_context>(object))); break;
        case clproto::HandleKind::device: static_cast<void>(on_clReleaseDevice(static_cast<cl_device_id>(object))); break;
        case clproto::HandleKind::platform: return;
        }
    }
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
    /* What this context imported is released by HostShares as the session
     * ends, before this: the registry holds no shared cl_mem by now. */
    stop_pacing();
    /* The module kept is of a program the guest may have released already;
     * its last reference goes with the guest's objects. */
    il_.forget();
    handles_.release_all(release_order, sizeof release_order / sizeof release_order[0],
                         [this](uint64_t, clproto::HandleKind kind, void *object, uint32_t times) {
                             let_go(kind, object, times);
                         });
    /* Before the ranges go: a release may free an allocation, and nothing
     * afterwards may resolve one. */
    svm_.forget_all();
    mapped_.forget_all();
    if (queue_ != nullptr) {
        static_cast<void>(::clReleaseCommandQueue(static_cast<cl_command_queue>(queue_)));
        queue_ = nullptr;
    }
    context_ = nullptr;
    /* Every event callback still to come goes stale here. The host cannot be
     * told to stop calling them, so being called and doing nothing is what
     * they do from now on. */
    events_.shut_down();
    /* The vehicle last: its context is what deletes what was made for the
     * guest's GL objects. */
    close_vehicle();
    reaches_gl_ = false;
    bound_ = nullptr;
    imported_ = nullptr;
    ran_ = 0u;
    lost_ = false;
}

cl_int OpenClBackend::on_clEnqueueNDRangeKernel(cl_command_queue command_queue, cl_kernel kernel, cl_uint work_dim,
                                                const size_t *global_work_offset, const size_t *global_work_size,
                                                const size_t *local_work_size, cl_uint num_events_in_wait_list,
                                                const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueNDRangeKernel");
    const cl_int enqueued = RealDispatch::on_clEnqueueNDRangeKernel(command_queue, kernel, work_dim,
                                                                    global_work_offset, global_work_size,
                                                                    local_work_size, num_events_in_wait_list,
                                                                    event_wait_list, event);
    if (enqueued == CL_SUCCESS) pace(command_queue);
    return enqueued;
}

cl_int OpenClBackend::on_clEnqueueTask(cl_command_queue command_queue, cl_kernel kernel,
                                       cl_uint num_events_in_wait_list, const cl_event *event_wait_list,
                                       cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueTask");
    const cl_int enqueued =
        RealDispatch::on_clEnqueueTask(command_queue, kernel, num_events_in_wait_list, event_wait_list, event);
    if (enqueued == CL_SUCCESS) pace(command_queue);
    return enqueued;
}

cl_int OpenClBackend::on_clFlush(cl_command_queue command_queue)
{
    AV_SCOPE("OpenClBackend::on_clFlush");
    const cl_int flushed = RealDispatch::on_clFlush(command_queue);
    if (command_queue == paced_queue_) pacer_.flushed();
    look_at_marker();
    return flushed;
}

cl_int OpenClBackend::on_clFinish(cl_command_queue command_queue)
{
    AV_SCOPE("OpenClBackend::on_clFinish");
    const cl_int finished = RealDispatch::on_clFinish(command_queue);
    if (command_queue == paced_queue_) pacer_.flushed();
    look_at_marker();
    return finished;
}

void OpenClBackend::pace(cl_command_queue queue)
{
    AV_SCOPE("OpenClBackend::pace");
    look_at_marker();
    if (queue != paced_queue_) {
        if (paced_queue_ != nullptr) {
            static_cast<void>(::clFlush(static_cast<cl_command_queue>(paced_queue_)));
            static_cast<void>(::clReleaseCommandQueue(static_cast<cl_command_queue>(paced_queue_)));
        }
        pacer_.flushed();
        paced_queue_ = ::clRetainCommandQueue(queue) == CL_SUCCESS ? queue : nullptr;
    }
    if (!pacer_.kernel()) return;
    cl_event marker = nullptr;
    if (!pacer_.timing() && ::clEnqueueMarkerWithWaitList(queue, 0u, nullptr, &marker) == CL_SUCCESS) {
        timespec now = {};
        clock_gettime(CLOCK_MONOTONIC, &now);
        marker_ = marker;
        pacer_.time_from(static_cast<uint64_t>(now.tv_sec) * 1000000000u + static_cast<uint64_t>(now.tv_nsec));
    }
    static_cast<void>(::clFlush(queue));
    pacer_.flushed();
}

/* A marker that failed is a group that ended, as far as timing it goes. */
void OpenClBackend::look_at_marker()
{
    AV_SCOPE("OpenClBackend::look_at_marker");
    if (marker_ == nullptr) return;
    cl_int status = CL_QUEUED;
    if (::clGetEventInfo(static_cast<cl_event>(marker_), CL_EVENT_COMMAND_EXECUTION_STATUS, sizeof status, &status,
                         nullptr) == CL_SUCCESS &&
        status > CL_COMPLETE)
        return;
    timespec now = {};
    clock_gettime(CLOCK_MONOTONIC, &now);
    pacer_.timed(static_cast<uint64_t>(now.tv_sec) * 1000000000u + static_cast<uint64_t>(now.tv_nsec));
    static_cast<void>(::clReleaseEvent(static_cast<cl_event>(marker_)));
    marker_ = nullptr;
}

void OpenClBackend::stop_pacing()
{
    AV_SCOPE("OpenClBackend::stop_pacing");
    if (marker_ != nullptr) static_cast<void>(::clReleaseEvent(static_cast<cl_event>(marker_)));
    if (paced_queue_ != nullptr) static_cast<void>(::clReleaseCommandQueue(static_cast<cl_command_queue>(paced_queue_)));
    marker_ = nullptr;
    paced_queue_ = nullptr;
    pacer_ = KernelPacer();
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
    /* Room first: an allocation this could not remember is one whose address
     * could never be judged. clSVMAlloc answers a lack of memory with null. */
    if (svm_.make_room() != Status::ok) return nullptr;
    void *const allocated = ::clSVMAlloc(context, flags, size, alignment);
    if (allocated == nullptr) return nullptr;
    if (svm_.remember(allocated, static_cast<uint64_t>(size), 0u) != Status::ok) { AV_UNTESTABLE("room was made above, and no host allocates no bytes or past the end of the address space");
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
    if (mapped_.make_room() != Status::ok) {
        if (errcode_ret != nullptr) *errcode_ret = CL_OUT_OF_HOST_MEMORY;
        return nullptr;
    }
    void *const mapped =
        ::clEnqueueMapBuffer(command_queue, buffer, blocking_map, map_flags, offset, size,
                             num_events_in_wait_list, event_wait_list, event, errcode_ret);
    if (mapped == nullptr) return nullptr;
    /* A mapping this cannot remember - no bytes, or past the end of the
     * address space - is one whose unmap could never be judged, and a buffer
     * that can never be unmapped is worse than a map that failed: the guest
     * would hold it for the life of the context. */
    if (mapped_.remember(mapped, static_cast<uint64_t>(size),
                         static_cast<uint64_t>(reinterpret_cast<uintptr_t>(buffer))) != Status::ok) { AV_UNTESTABLE("room was made above, and no host maps no bytes or past the end of the address space");
        static_cast<void>(::clEnqueueUnmapMemObject(command_queue, buffer, mapped, 0u, nullptr,
                                                    nullptr));
        if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_VALUE;
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
    static_cast<void>(blocking_read);
    cl_int served = CL_SUCCESS;
    if (num_events_in_wait_list == 0u && event == nullptr && !arena_.holds(ptr) &&
        read_ahead_.read(command_queue, buffer, offset, size, ptr, served))
        return served;
    return RealDispatch::on_clEnqueueReadBuffer(command_queue, buffer, CL_TRUE, offset, size, ptr,
                                                num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clEnqueueReadImage(cl_command_queue command_queue, cl_mem image, cl_bool blocking_read,
                                            const size_t *origin, const size_t *region, size_t row_pitch,
                                            size_t slice_pitch, void *ptr, cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueReadImage");
    static_cast<void>(blocking_read);
    return RealDispatch::on_clEnqueueReadImage(command_queue, image, CL_TRUE, origin, region, row_pitch,
                                               slice_pitch, ptr, num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clEnqueueReadBufferRect(
    cl_command_queue command_queue, cl_mem buffer, cl_bool blocking_read, const size_t *buffer_origin,
    const size_t *host_origin, const size_t *region, size_t buffer_row_pitch, size_t buffer_slice_pitch,
    size_t host_row_pitch, size_t host_slice_pitch, void *ptr, cl_uint num_events_in_wait_list,
    const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueReadBufferRect");
    static_cast<void>(blocking_read);
    const bool into_arena = arena_.holds(ptr);
    if (!into_arena || !bounced_.bounces()) {
        const cl_int direct = RealDispatch::on_clEnqueueReadBufferRect(
            command_queue, buffer, CL_TRUE, buffer_origin, host_origin, region, buffer_row_pitch,
            buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list, event_wait_list,
            event);
        if (!into_arena || !bounced_.refusal(direct)) return direct;
    }
    return bounced_.read(command_queue, buffer, buffer_origin, host_origin, region, buffer_row_pitch,
                         buffer_slice_pitch, host_row_pitch, host_slice_pitch, ptr, num_events_in_wait_list,
                         event_wait_list, event);
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

cl_mem OpenClBackend::on_clCreateFromGLBuffer(cl_context context, cl_mem_flags flags, cl_GLuint bufobj,
                                              cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateFromGLBuffer");
    if (gl_ != nullptr) return gl_->from_buffer(context, flags, bufobj, errcode_ret);
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_CONTEXT;
    return nullptr;
}

cl_mem OpenClBackend::on_clCreateFromGLTexture(cl_context context, cl_mem_flags flags, cl_GLenum target,
                                               cl_GLint miplevel, cl_GLuint texture, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateFromGLTexture");
    if (gl_ != nullptr) return gl_->from_texture(context, flags, target, miplevel, texture, errcode_ret);
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_CONTEXT;
    return nullptr;
}

cl_mem OpenClBackend::on_clCreateFromGLTexture2D(cl_context context, cl_mem_flags flags, cl_GLenum target,
                                                 cl_GLint miplevel, cl_GLuint texture, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateFromGLTexture2D");
    return on_clCreateFromGLTexture(context, flags, target, miplevel, texture, errcode_ret);
}

cl_mem OpenClBackend::on_clCreateFromGLTexture3D(cl_context context, cl_mem_flags flags, cl_GLenum target,
                                                 cl_GLint miplevel, cl_GLuint texture, cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateFromGLTexture3D");
    return on_clCreateFromGLTexture(context, flags, target, miplevel, texture, errcode_ret);
}

cl_mem OpenClBackend::on_clCreateFromGLRenderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                                    cl_int *errcode_ret)
{
    AV_SCOPE("OpenClBackend::on_clCreateFromGLRenderbuffer");
    if (gl_ != nullptr) return gl_->from_renderbuffer(context, flags, renderbuffer, errcode_ret);
    if (errcode_ret != nullptr) *errcode_ret = CL_INVALID_CONTEXT;
    return nullptr;
}

cl_int OpenClBackend::on_clGetGLObjectInfo(cl_mem memobj, cl_gl_object_type *gl_object_type,
                                           cl_GLuint *gl_object_name)
{
    AV_SCOPE("OpenClBackend::on_clGetGLObjectInfo");
    if (gl_ == nullptr) return CL_INVALID_GL_OBJECT;
    return gl_->object_info(memobj, gl_object_type, gl_object_name);
}

cl_int OpenClBackend::on_clGetGLTextureInfo(cl_mem memobj, cl_gl_texture_info param_name, size_t param_value_size,
                                            void *param_value, size_t *param_value_size_ret)
{
    AV_SCOPE("OpenClBackend::on_clGetGLTextureInfo");
    if (gl_ == nullptr) return CL_INVALID_GL_OBJECT;
    return gl_->texture_info(memobj, param_name, param_value_size, param_value, param_value_size_ret);
}

cl_int OpenClBackend::on_clEnqueueAcquireGLObjects(cl_command_queue command_queue, cl_uint num_objects,
                                                   const cl_mem *mem_objects, cl_uint num_events_in_wait_list,
                                                   const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueAcquireGLObjects");
    if (gl_ == nullptr) return CL_INVALID_CONTEXT;
    return gl_->acquire(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clEnqueueReleaseGLObjects(cl_command_queue command_queue, cl_uint num_objects,
                                                   const cl_mem *mem_objects, cl_uint num_events_in_wait_list,
                                                   const cl_event *event_wait_list, cl_event *event)
{
    AV_SCOPE("OpenClBackend::on_clEnqueueReleaseGLObjects");
    if (gl_ == nullptr) return CL_INVALID_CONTEXT;
    return gl_->release(command_queue, num_objects, mem_objects, num_events_in_wait_list, event_wait_list, event);
}

cl_int OpenClBackend::on_clGetEventInfo(cl_event event, cl_event_info param_name, size_t param_value_size,
                                        void *param_value, size_t *param_value_size_ret)
{
    AV_SCOPE("OpenClBackend::on_clGetEventInfo");
    const cl_int asked =
        RealDispatch::on_clGetEventInfo(event, param_name, param_value_size, param_value, param_value_size_ret);
    if (asked != CL_SUCCESS || param_name != CL_EVENT_COMMAND_EXECUTION_STATUS || param_value == nullptr ||
        param_value_size < sizeof(cl_int) || *static_cast<const cl_int *>(param_value) != CL_QUEUED)
        return asked;
    /* A user event has no queue, and its status is the guest's to set. */
    cl_command_queue queue = nullptr;
    if (::clGetEventInfo(event, CL_EVENT_COMMAND_QUEUE, sizeof queue, &queue, nullptr) == CL_SUCCESS &&
        queue != nullptr)
        static_cast<void>(on_clFlush(queue));
    return asked;
}

cl_int OpenClBackend::on_clSetEventCallback(cl_event event, cl_int command_exec_callback_type)
{
    AV_SCOPE("OpenClBackend::on_clSetEventCallback");
    /* A device that gave this library nothing to signal with cannot wake the
     * guest, and a callback nobody hears is worse than a refusal: the guest
     * would wait on it forever. */
    if (!events_.bound()) return CL_INVALID_OPERATION;
    ClEventRegistration *const slot = events_.take();
    if (slot == nullptr) return CL_OUT_OF_HOST_MEMORY;
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
                                  const SharedImageShape &shape,
                                  uint64_t &wire_handle, uint64_t &detail)
{
    AV_SCOPE("OpenClBackend::import_object");
    wire_handle = 0u;
    detail = 0u;
    /* Memory only, and only as a dma-buf: a fence is a host semaphore that
     * OpenCL names through a different extension this host has not got, and an
     * opaque fd is another driver's private handle that OpenGL cannot read
     * (research.md §7). */
    if (kind != AV_SHARED_MEMORY || handle_kind != AV_SHARED_HANDLE_DMA_BUF || fd < 0 || size == 0u)
        return false;
    if (!imports_ || context_ == nullptr) return false;
    if (acquire_queue() == nullptr) { AV_UNTESTABLE("no host beside a test here refuses a command queue"); return false; }
    uint32_t memory = 0u, buffer = 0u;
    if (!buffers_.make(fd, size, memory, buffer)) return false; // AV_UNTESTABLE("a descriptor this host's GL refuses; an ordinary fd is taken and then faults in the driver")
    cl_int   made = CL_SUCCESS;
    cl_mem const shared =
        ::clCreateFromGLBuffer(static_cast<cl_context>(context_), CL_MEM_READ_WRITE, buffer, &made);
    if (shared == nullptr) { AV_UNTESTABLE("no host beside a test here refuses a buffer it imported itself");
        buffers_.destroy(memory, buffer);
        return false;
    }
    /* Acquired once and held: the acquire is the rule about OpenGL and OpenCL
     * taking turns over one object, and the GL context here is an import
     * vehicle that never draws - so OpenCL's turn is the whole of the object's
     * life, and no enqueue has to be bracketed. Proven on this host: a write
     * and a read across enqueues under one acquire, and what OpenCL wrote read
     * back through the exporting Vulkan device's own mapping. */
    const bool taken = ::clEnqueueAcquireGLObjects(static_cast<cl_command_queue>(queue_), 1u, &shared, 0u, nullptr, nullptr) == CL_SUCCESS && ::clFinish(static_cast<cl_command_queue>(queue_)) == CL_SUCCESS;  // AV_UNTESTABLE("no host beside a test here refuses an acquire")
    /* A Direct3D 11 texture's memory is that image, viewed over the buffer at
     * the pitch the GPU laid it out with: nothing is copied either way. */
    cl_mem const named = taken ? image_over(shared, shape) : nullptr;
    /* Named in the guest's commands from now on, as any cl_mem the host made
     * is: one the registry did not hold would resolve to nothing in every
     * enqueue that names it. */
    if (!taken || hold(clproto::HandleKind::mem, named) != Status::ok) { AV_UNTESTABLE("the acquire above, and a table that cannot grow");
        /* An image holds the buffer's one reference: letting it go lets both
         * go. */
        cl_mem const held = named != nullptr ? named : shared;
        static_cast<void>(handles_.release(number_of(held), clproto::HandleKind::mem));
        static_cast<void>(::clReleaseMemObject(held));
        buffers_.destroy(memory, buffer);
        return false;
    }
    wire_handle = reinterpret_cast<uint64_t>(named);
    /* The GL objects behind it, which the release deletes: HostShares keeps
     * them with the hold. */
    detail = (uint64_t{buffer} << 32u) | memory;
    return true;
}

/* The image over `buffer` that `shape` describes, holding a reference to the
 * buffer of its own; `buffer` itself where the memory is no image OpenCL can
 * name, or the host will not make one over it. */
cl_mem OpenClBackend::image_over(cl_mem buffer, const SharedImageShape &shape)
{
    AV_SCOPE("OpenClBackend::image_over");
    const DxgiFormat format(shape.format());
    if (!shape.described() || !format.cl_image_on_host()) return buffer;
    const cl_image_format named = {static_cast<cl_channel_order>(format.cl_order()),
                                   static_cast<cl_channel_type>(format.cl_type())};
    cl_image_desc         described = {};
    described.image_type = CL_MEM_OBJECT_IMAGE2D;
    described.image_width = shape.width();
    described.image_height = shape.height();
    described.image_row_pitch = shape.pitch();
    described.buffer = buffer;
    cl_int       made = CL_SUCCESS;
    cl_mem const image = ::clCreateImage(static_cast<cl_context>(context_), CL_MEM_READ_WRITE, &named, &described,
                                         nullptr, &made);
    if (image == nullptr) {
        StdioLog sink(stderr);
        DriverLog(sink, "virtio-gpu-av: opencl: ")
            .note_hex("a shared texture is opened as bytes, no image over its memory: error",
                      static_cast<uint32_t>(-made), 8u);
        return buffer;
    }
    /* The image holds the buffer: the reference the import took is its. */
    static_cast<void>(::clReleaseMemObject(buffer));
    return image;
}

void OpenClBackend::release_object(uint32_t, uint64_t wire_handle, uint64_t detail)
{
    AV_SCOPE("OpenClBackend::release_object");
    const auto buffer = static_cast<uint32_t>(detail >> 32u);
    const auto memory = static_cast<uint32_t>(detail);
    /* The reverse of the making: the turn given back, then the cl_mem, then
     * the buffer that was its storage, and the memory object last. */
    /* The queue is the import's own and outlives every row it made, so a row
     * to give back is a queue to give it back on. */
    cl_mem const named = reinterpret_cast<cl_mem>(wire_handle);
    static_cast<void>(handles_.release(number_of(named), clproto::HandleKind::mem));
    /* An image's turn was taken on the buffer under it, which the image
     * holds: the buffer is what is given back, and the image let go first. */
    cl_mem shared = named;
    cl_mem under = nullptr;
    if (::clGetMemObjectInfo(named, CL_MEM_ASSOCIATED_MEMOBJECT, sizeof under, &under, nullptr) == CL_SUCCESS &&
        under != nullptr) {
        static_cast<void>(::clRetainMemObject(under));
        static_cast<void>(::clReleaseMemObject(named));
        shared = under;
    }
    static_cast<void>(::clEnqueueReleaseGLObjects(static_cast<cl_command_queue>(queue_), 1u, &shared, 0u,
                                                  nullptr, nullptr));
    static_cast<void>(::clFinish(static_cast<cl_command_queue>(queue_)));
    static_cast<void>(::clReleaseMemObject(shared));
    buffers_.destroy(memory, buffer);
}

}  // namespace av
