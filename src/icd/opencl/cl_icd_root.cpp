#include "icd/opencl/cl_icd_root.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {

/* What an entry answers for a pointer that is not an object of the kind it
 * wanted. Each is the API's own, because an application that passed the wrong
 * object is entitled to be told which one. */
constexpr cl_int wrong_kind(ClObject::Kind kind)
{
    switch (kind) {
    case ClObject::Kind::platform: return CL_INVALID_PLATFORM;
    case ClObject::Kind::device: return CL_INVALID_DEVICE;
    case ClObject::Kind::context: return CL_INVALID_CONTEXT;
    case ClObject::Kind::command_queue: return CL_INVALID_COMMAND_QUEUE;
    case ClObject::Kind::program: return CL_INVALID_PROGRAM;
    case ClObject::Kind::mem: return CL_INVALID_MEM_OBJECT;
    case ClObject::Kind::sampler: return CL_INVALID_SAMPLER;
    case ClObject::Kind::kernel: return CL_INVALID_KERNEL;
    default: return CL_INVALID_EVENT;
    }
}

void report_error(cl_int *error, cl_int value)
{
    AV_SCOPE("report_error");
    if (error != nullptr) *error = value;
}

}  // namespace

ClIcdRoot::ClIcdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock,
                     icd::ThreadLock &send_lock, icd::LogSink &log, ClIcdBinding &binding)
    : lock_(lock), log_(log), binding_(binding), dispatch_(), platform_(),
      extensions_(reported_extensions, 0u), objects_(memory, dispatch_.table()), connection_(kmt),
      contexts_(memory), memory_(memory), send_lock_(send_lock), device_(0u),
      device_object_(nullptr)
{
    AV_SCOPE("ClIcdRoot::ClIcdRoot");
    /* The table is this ICD's entries where it has them and the generated
     * refusal everywhere else. The loader reads a slot and calls through it
     * without checking, so a null one is a jump into nothing. */
    dispatch_.fill();
    platform_.dispatch_through(dispatch_.table());
    binding_.bind(*this);
}

ClIcdRoot::~ClIcdRoot()
{
    AV_SCOPE("ClIcdRoot::~ClIcdRoot");
    binding_.unbind();
}

const cl_icd_dispatch &ClIcdRoot::dispatch() const
{
    AV_SCOPE("ClIcdRoot::dispatch");
    return dispatch_.table();
}

cl_int ClIcdRoot::platform_ids(cl_uint num_entries, cl_platform_id *platforms,
                               cl_uint *num_platforms)
{
    AV_SCOPE("ClIcdRoot::platform_ids");
    /* The loader's own shape, and the API's: a count, or as many as asked
     * for, and never both zero. Nothing is opened. */
    if ((platforms == nullptr && num_platforms == nullptr) ||
        (platforms != nullptr && num_entries == 0u)) {
        return CL_INVALID_VALUE;
    }
    if (platforms != nullptr) platforms[0] = platform_.as_handle();
    if (num_platforms != nullptr) *num_platforms = 1u;
    return CL_SUCCESS;
}

cl_int ClIcdRoot::platform_info(cl_platform_id platform, cl_platform_info name, size_t value_bytes,
                                void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::platform_info");
    if (name == CL_PLATFORM_EXTENSIONS) {
        /* Before a device exists there is no host to intersect with, and the
         * loader asks this while enumerating. Nothing is reported that
         * nothing proves, so an empty list is the honest answer either way
         * until a row exists. */
        static const char none[] = "";
        if (value != nullptr) {
            if (value_bytes < sizeof none) return CL_INVALID_VALUE;
            static_cast<char *>(value)[0] = '\0';
        }
        if (size_ret != nullptr) *size_ret = sizeof none;
        return CL_SUCCESS;
    }
    return platform_.info(platform, name, value_bytes, value, size_ret);
}

void *ClIcdRoot::extension_function(const char *name)
{
    AV_SCOPE("ClIcdRoot::extension_function");
    if (name == nullptr) return nullptr;
    /* The one name the loader asks for, and the only one this ICD answers:
     * everything else it wants, it reaches through the dispatch table. */
    const char wanted[] = "clIcdGetPlatformIDsKHR";
    size_t index = 0u;
    while (wanted[index] != '\0' && name[index] == wanted[index]) ++index;
    if (wanted[index] != '\0' || name[index] != '\0') return nullptr;
    return dispatch_.icd_get_platform_ids();
}

Status ClIcdRoot::open_device()
{
    AV_SCOPE("ClIcdRoot::open_device");
    if (device_object_ != nullptr) return Status::ok;
    /* The ICD's own context: what its device queries are asked over, and what
     * holds the adapter open for as long as a device exists. An application's
     * contexts are its own, made beside this one. */
    GuestClContext *context = nullptr;
    const Status    made = contexts_.create(context, connection_, memory_, send_lock_, log_);
    if (made != Status::ok) {
        log_.write("rvkcl: no room for the ICD's own context");
        return made;
    }
    const Status opened = context->make();
    if (opened != Status::ok) {
        log_.write("rvkcl: the ICD's own context could not be made");
        static_cast<void>(contexts_.destroy(context));
        return opened;
    }
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) {
        static_cast<void>(contexts_.destroy(context));
        return Status::not_supported;
    }
    /* What the host has, asked once. The device it answers is the one this
     * ICD shows, because the executor bound one GPU to this context. */
    uint64_t host_platform = 0u;
    cl_uint  platforms = 0u;
    if (av::clproto::rvk_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) != CL_SUCCESS ||
        platforms == 0u || host_platform == 0u) {
        static_cast<void>(contexts_.destroy(context));
        return Status::not_supported;
    }
    uint64_t host_device = 0u;
    cl_uint  devices = 0u;
    if (av::clproto::rvk_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device,
                                        &devices) != CL_SUCCESS ||
        devices == 0u || host_device == 0u) {
        static_cast<void>(contexts_.destroy(context));
        return Status::not_supported;
    }
    /* The adapter this device is on, which is what CL_DEVICE_LUID_KHR names:
     * the host GPU's own LUID would send an application looking for a device
     * that is not the one it has. */
    uint64_t adapter_luid = 0u;
    static_cast<void>(connection_.device(0u).kmt().adapter_luid(
        connection_.device(0u).adapter(), adapter_luid));
    device_.observe_luid(adapter_luid);
    ClObject *object = nullptr;
    const Status held = objects_.make(ClObject::Kind::device, host_device, context, nullptr, object);
    if (held != Status::ok) {
        static_cast<void>(contexts_.destroy(context));
        return held;
    }
    device_object_ = object;
    /* What the device says about itself, capped at what this ICD implements. */
    char text[64] = {};
    size_t bytes = 0u;
    if (av::clproto::rvk_clGetDeviceInfo(*wire, host_device, CL_DEVICE_VERSION, sizeof text, text,
                                         &bytes) == CL_SUCCESS) {
        static_cast<void>(device_.observe_version(text, bytes));
    }
    if (av::clproto::rvk_clGetDeviceInfo(*wire, host_device, CL_DEVICE_OPENCL_C_VERSION, sizeof text,
                                         text, &bytes) == CL_SUCCESS) {
        static_cast<void>(device_.observe_c_version(text, bytes));
    }
    cl_device_svm_capabilities svm = 0u;
    if (av::clproto::rvk_clGetDeviceInfo(*wire, host_device, CL_DEVICE_SVM_CAPABILITIES, sizeof svm,
                                         &svm, &bytes) == CL_SUCCESS) {
        device_.observe_svm(svm);
    }
    return Status::ok;
}

cl_int ClIcdRoot::device_ids(cl_platform_id platform, cl_device_type type, cl_uint num_entries,
                             cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("ClIcdRoot::device_ids");
    if (!platform_.names_this(platform)) return CL_INVALID_PLATFORM;
    if ((devices == nullptr && num_devices == nullptr) ||
        (devices != nullptr && num_entries == 0u)) {
        return CL_INVALID_VALUE;
    }
    /* This adapter's device is a GPU, so a type that does not include one
     * finds nothing - the API's own answer, and not an error. */
    if (type == 0u) return CL_INVALID_DEVICE_TYPE;
    const bool wants_gpu = (type & (CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_DEFAULT)) != 0u ||
                           type == CL_DEVICE_TYPE_ALL;
    if (!wants_gpu) {
        if (num_devices != nullptr) *num_devices = 0u;
        return CL_DEVICE_NOT_FOUND;
    }
    lock_.acquire();
    const Status opened = open_device();
    lock_.release();
    if (opened != Status::ok) {
        if (num_devices != nullptr) *num_devices = 0u;
        return CL_DEVICE_NOT_FOUND;
    }
    if (devices != nullptr) devices[0] = reinterpret_cast<cl_device_id>(device_object_);
    if (num_devices != nullptr) *num_devices = 1u;
    return CL_SUCCESS;
}

cl_int ClIcdRoot::device_info(cl_device_id device, cl_device_info name, size_t value_bytes,
                              void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::device_info");
    ClObject *const object = judged(device, ClObject::Kind::device);
    if (object == nullptr) return CL_INVALID_DEVICE;
    /* The four this ICD decides name the rvkgpu adapter rather than the host
     * GPU, or cap what it implements; everything else is the host's own. */
    if (device_.decides(name)) return device_.info(name, value_bytes, value, size_ret);
    ClientTransport *const wire = wire_of(*object);
    if (wire == nullptr) return CL_INVALID_DEVICE;
    return av::clproto::rvk_clGetDeviceInfo(*wire, object->wire(), name, value_bytes, value,
                                            size_ret);
}

ClObject *ClIcdRoot::judged(const void *pointer, ClObject::Kind kind)
{
    AV_SCOPE("ClIcdRoot::judged");
    return objects_.made(pointer, kind);
}

GuestClContext *ClIcdRoot::context_of(ClObject &object)
{
    AV_SCOPE("ClIcdRoot::context_of");
    return object.context();
}

ClientTransport *ClIcdRoot::wire_of(ClObject &object)
{
    AV_SCOPE("ClIcdRoot::wire_of");
    GuestClContext *const context = context_of(object);
    return context != nullptr ? context->wire() : nullptr;
}

cl_int ClIcdRoot::refusal_for(ClObject::Kind kind) const
{
    AV_SCOPE("ClIcdRoot::refusal_for");
    return wrong_kind(kind);
}

void *ClIcdRoot::wrap(ClObject::Kind kind, uint64_t wire, GuestClContext *context, ClObject *parent,
                      cl_int *error)
{
    AV_SCOPE("ClIcdRoot::wrap");
    ClObject *object = nullptr;
    if (objects_.make(kind, wire, context, parent, object) != Status::ok) {
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    report_error(error, CL_SUCCESS);
    return object;
}

cl_context ClIcdRoot::create_context(const cl_device_id *devices, cl_uint num_devices, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_context");
    if (devices == nullptr || num_devices == 0u) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    /* One device, and it has to be this ICD's. */
    if (num_devices != 1u || judged(devices[0], ClObject::Kind::device) == nullptr) {
        report_error(error, CL_INVALID_DEVICE);
        return nullptr;
    }
    lock_.acquire();
    GuestClContext *context = nullptr;
    Status          made = contexts_.create(context, connection_, memory_, send_lock_, log_);
    if (made == Status::ok) made = context->make();
    if (made != Status::ok) {
        if (context != nullptr) static_cast<void>(contexts_.destroy(context));
        lock_.release();
        report_error(error, made == Status::no_memory ? CL_OUT_OF_HOST_MEMORY
                                                      : CL_DEVICE_NOT_AVAILABLE);
        return nullptr;
    }
    ClientTransport *const wire = context->wire();
    uint64_t              host_platform = 0u;
    cl_uint               platforms = 0u;
    uint64_t              host_device = 0u;
    cl_uint               found = 0u;
    cl_int                host_error = CL_SUCCESS;
    uint64_t              host_context = 0u;
    if (wire != nullptr &&
        av::clproto::rvk_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) == CL_SUCCESS &&
        av::clproto::rvk_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device,
                                        &found) == CL_SUCCESS) {
        host_context = av::clproto::rvk_clCreateContext(*wire, nullptr, 1u, &host_device,
                                                        &host_error);
    }
    if (host_context == 0u) {
        static_cast<void>(contexts_.destroy(context));
        lock_.release();
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_DEVICE_NOT_AVAILABLE);
        return nullptr;
    }
    void *const wrapped = wrap(ClObject::Kind::context, host_context, context, nullptr, error);
    if (wrapped == nullptr) static_cast<void>(contexts_.destroy(context));
    lock_.release();
    return static_cast<cl_context>(wrapped);
}

cl_int ClIcdRoot::retain_context(cl_context context)
{
    AV_SCOPE("ClIcdRoot::retain_context");
    return retain(context, ClObject::Kind::context);
}

cl_int ClIcdRoot::release_context(cl_context context)
{
    AV_SCOPE("ClIcdRoot::release_context");
    return release(context, ClObject::Kind::context);
}

cl_int ClIcdRoot::retain(void *object, ClObject::Kind kind)
{
    AV_SCOPE("ClIcdRoot::retain");
    lock_.acquire();
    ClObject *const held = judged(object, kind);
    if (held != nullptr) held->retain();
    lock_.release();
    return held != nullptr ? CL_SUCCESS : refusal_for(kind);
}

cl_int ClIcdRoot::release(void *object, ClObject::Kind kind)
{
    AV_SCOPE("ClIcdRoot::release");
    lock_.acquire();
    ClObject *const held = judged(object, kind);
    if (held == nullptr) {
        lock_.release();
        return refusal_for(kind);
    }
    /* The application's count is this ICD's. The host is told once, when the
     * last of them goes; a context takes its own transport with it, and the
     * host's objects go with that. */
    GuestClContext *const context = held->context();
    const uint64_t        wire_handle = held->wire();
    const bool            last = held->references() == 1u;
    if (last && kind != ClObject::Kind::context && context != nullptr) {
        ClientTransport *const wire = context->wire();
        if (wire != nullptr) {
            switch (kind) {
            case ClObject::Kind::command_queue:
                static_cast<void>(av::clproto::rvk_clReleaseCommandQueue(*wire, wire_handle));
                break;
            case ClObject::Kind::mem:
                static_cast<void>(av::clproto::rvk_clReleaseMemObject(*wire, wire_handle));
                break;
            case ClObject::Kind::program:
                static_cast<void>(av::clproto::rvk_clReleaseProgram(*wire, wire_handle));
                break;
            case ClObject::Kind::kernel:
                static_cast<void>(av::clproto::rvk_clReleaseKernel(*wire, wire_handle));
                break;
            case ClObject::Kind::event:
                static_cast<void>(av::clproto::rvk_clReleaseEvent(*wire, wire_handle));
                break;
            default: break;
            }
        }
    }
    const bool held_still = !last;
    static_cast<void>(objects_.release(*held));
    if (!held_still && kind == ClObject::Kind::context && context != nullptr) {
        /* The context's own objects went with it in the table; its transport
         * and everything the host held on it go now. */
        static_cast<void>(contexts_.destroy(context));
    }
    lock_.release();
    return CL_SUCCESS;
}

void ClIcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("ClIcdRoot::report");
    writer.open("clicd", 0u);
    writer.flag("device", device_object_ != nullptr);
    objects_.report(writer);
    connection_.report(writer);
    writer.close();
}

bool ClIcdRoot::invariants_hold() const
{
    AV_SCOPE("ClIcdRoot::invariants_hold");
    if (!platform_.invariants_hold() || !extensions_.invariants_hold()) return false;
    if (!objects_.invariants_hold() || !connection_.invariants_hold()) return false;
    if (!device_.invariants_hold()) return false;
    /* A platform the loader would drop is one this ICD must never hand out. */
    return platform_.dispatchable();
}

cl_command_queue ClIcdRoot::create_queue(cl_context context, cl_device_id device, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_queue");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (judged(device, ClObject::Kind::device) == nullptr) {
        report_error(error, CL_INVALID_DEVICE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    /* The host's own device, which is what its context was made on: the
     * application's cl_device_id names this ICD's, and the two are one GPU. */
    ClObject *const host_device = device_object_;
    if (host_device == nullptr) {
        report_error(error, CL_INVALID_DEVICE);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t queue = av::clproto::rvk_clCreateCommandQueueWithProperties(
        *wire, held->wire(), host_device->wire(), nullptr, &host_error);
    if (queue == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_command_queue>(
        wrap(ClObject::Kind::command_queue, queue, held->context(), held, error));
}

cl_mem ClIcdRoot::create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                void *host_pointer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_buffer");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (bytes == 0u) {
        report_error(error, CL_INVALID_BUFFER_SIZE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t buffer =
        av::clproto::rvk_clCreateBuffer(*wire, held->wire(), flags, bytes, host_pointer, &host_error);
    if (buffer == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_mem>(wrap(ClObject::Kind::mem, buffer, held->context(), held, error));
}

cl_program ClIcdRoot::create_program(cl_context context, cl_uint count, const char **sources,
                                     const size_t *lengths, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_program");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (count == 0u || sources == nullptr) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t program = av::clproto::rvk_clCreateProgramWithSource(*wire, held->wire(), count,
                                                                       sources, lengths, &host_error);
    if (program == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_program>(wrap(ClObject::Kind::program, program, held->context(), held, error));
}

cl_int ClIcdRoot::build_program(cl_program program, const char *options)
{
    AV_SCOPE("ClIcdRoot::build_program");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* Built for the one device this ICD shows; a null list is what the API
     * means by "every device of the context", which is the same device. */
    return av::clproto::rvk_clBuildProgram(*wire, held->wire(), 0u, nullptr, options);
}

cl_kernel ClIcdRoot::create_kernel(cl_program program, const char *name, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_kernel");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) {
        report_error(error, CL_INVALID_PROGRAM);
        return nullptr;
    }
    if (name == nullptr) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t kernel = av::clproto::rvk_clCreateKernel(*wire, held->wire(), name, &host_error);
    if (kernel == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_INVALID_KERNEL_NAME);
        return nullptr;
    }
    return static_cast<cl_kernel>(wrap(ClObject::Kind::kernel, kernel, held->context(), held, error));
}

cl_int ClIcdRoot::set_kernel_argument(cl_kernel kernel, cl_uint index, size_t bytes,
                                      const void *value)
{
    AV_SCOPE("ClIcdRoot::set_kernel_argument");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* An argument of the size of a cl_mem may be one: the application passes
     * the address of its own pointer, and what crosses is the number the host
     * knows that object by. Anything else is bytes. */
    if (bytes == sizeof(cl_mem) && value != nullptr) {
        cl_mem named = nullptr;
        const unsigned char *const from = static_cast<const unsigned char *>(value);
        unsigned char *const        into = reinterpret_cast<unsigned char *>(&named);
        for (size_t index_byte = 0u; index_byte < sizeof named; ++index_byte) {
            into[index_byte] = from[index_byte];
        }
        ClObject *const object = judged(named, ClObject::Kind::mem);
        if (object != nullptr) {
            const uint64_t handle = object->wire();
            return av::clproto::rvk_clSetKernelArg(*wire, held->wire(), index, sizeof handle,
                                                   &handle);
        }
    }
    return av::clproto::rvk_clSetKernelArg(*wire, held->wire(), index, bytes, value);
}

cl_int ClIcdRoot::enqueue_kernel(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                 const size_t *offset, const size_t *global, const size_t *local,
                                 cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_kernel");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(kernel, ClObject::Kind::kernel);
    if (named == nullptr) return CL_INVALID_KERNEL;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     produced = 0u;
    const cl_int ran = av::clproto::rvk_clEnqueueNDRangeKernel(
        *wire, held->wire(), named->wire(), dimensions, offset, global, local, 0u, nullptr,
        event != nullptr ? &produced : nullptr);
    if (ran == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return ran;
}

cl_int ClIcdRoot::enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                               size_t offset, size_t bytes, void *into, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_read");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (into == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     produced = 0u;
    const cl_int ran = av::clproto::rvk_clEnqueueReadBuffer(
        *wire, held->wire(), named->wire(), blocking, offset, bytes, into, 0u, nullptr,
        event != nullptr ? &produced : nullptr);
    if (ran == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return ran;
}

cl_int ClIcdRoot::enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                size_t offset, size_t bytes, const void *from, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_write");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     produced = 0u;
    const cl_int ran = av::clproto::rvk_clEnqueueWriteBuffer(
        *wire, held->wire(), named->wire(), blocking, offset, bytes, from, 0u, nullptr,
        event != nullptr ? &produced : nullptr);
    if (ran == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return ran;
}

cl_int ClIcdRoot::finish(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::finish");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* A synchronous command: the reply lands when the host has finished, and
     * the transport sleeps on the context's fence until it does. Nothing
     * polls. */
    return av::clproto::rvk_clFinish(*wire, held->wire());
}

cl_int ClIcdRoot::flush(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::flush");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::rvk_clFlush(*wire, held->wire());
}

}  // namespace av::clicd
