#include "icd/opencl/cl_icd_root.hpp"

#include "proto/cl_blob_backing.hpp"

#include "icd/common/sliced_wait.hpp"

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
                     icd::ThreadLock &send_lock, icd::LogSink &log,
                     icd::ServiceThread &watcher_thread, ClIcdBinding &binding)
    : lock_(lock), log_(log), binding_(binding), dispatch_(), platform_(),
      extensions_(reported_extensions, 0u), objects_(memory, dispatch_.table()), connection_(kmt),
      contexts_(memory), memory_(memory), send_lock_(send_lock), device_(0u),
      device_object_(nullptr), watcher_(watcher_thread, log, *this), waits_(kmt), reads_()
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
    /* CL_DEVICE_TYPE_ALL is every bit, so it carries the GPU bit too and
     * needs no case of its own; a test for it here could never be reached. */
    const bool wants_gpu = (type & (CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_DEFAULT)) != 0u;
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

void ClIcdRoot::give_back_to_host(ClientTransport &wire, ClObject::Kind kind, uint64_t handle)
{
    AV_SCOPE("ClIcdRoot::give_back_to_host");
    /* Zero is what a host answers when it made nothing, so there is nothing
     * to give back and the command would name no object. */
    if (handle == 0u) return;
    switch (kind) {
    case ClObject::Kind::command_queue:
        static_cast<void>(av::clproto::rvk_clReleaseCommandQueue(wire, handle));
        break;
    case ClObject::Kind::mem:
        static_cast<void>(av::clproto::rvk_clReleaseMemObject(wire, handle));
        break;
    case ClObject::Kind::program:
        static_cast<void>(av::clproto::rvk_clReleaseProgram(wire, handle));
        break;
    case ClObject::Kind::kernel:
        static_cast<void>(av::clproto::rvk_clReleaseKernel(wire, handle));
        break;
    case ClObject::Kind::event:
        static_cast<void>(av::clproto::rvk_clReleaseEvent(wire, handle));
        break;
    /* A context is torn down rather than released through here, and a
     * platform, a device and a sampler are never the host's to give back. */
    default: break;
    }
}

void *ClIcdRoot::wrap(ClObject::Kind kind, uint64_t wire, GuestClContext *context, ClObject *parent,
                      cl_int *error)
{
    AV_SCOPE("ClIcdRoot::wrap");
    ClObject *object = nullptr;
    if (objects_.make(kind, wire, context, parent, object) != Status::ok) {
        /* The host made it and the guest cannot keep it, so nobody will ever
         * name it again. Giving it back here is the only chance: the
         * application is about to be told the call failed, and an object it
         * was never handed is one it can never release. */
        ClientTransport *const transport = context != nullptr ? context->wire() : nullptr;
        if (transport != nullptr) give_back_to_host(*transport, kind, wire);
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    report_error(error, CL_SUCCESS);
    return object;
}

cl_int ClIcdRoot::judge(const uint64_t *properties, const uint64_t *allowed, uint32_t allowed_count,
                        ClPropertyList &read) const
{
    AV_SCOPE("ClIcdRoot::judge");
    /* A list with no terminator inside the bound is not a list this can read,
     * and reading past it is the one thing that must not happen. */
    if (!read.read(properties)) return CL_INVALID_PROPERTY;
    if (read.repeated()) return CL_INVALID_PROPERTY;
    for (uint32_t index = 0u; index < read.pairs(); ++index) {
        const uint64_t name = read.name_at(index);
        bool           known = false;
        for (uint32_t row = 0u; row < allowed_count && !known; ++row) known = allowed[row] == name;
        if (!known) return CL_INVALID_PROPERTY;
    }
    return CL_SUCCESS;
}

cl_context ClIcdRoot::create_context(const uint64_t *properties, const cl_device_id *devices,
                                     cl_uint num_devices, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_context");
    /* CL_CONTEXT_PLATFORM must be this platform if it is named at all; the
     * interop-sync flag is accepted and has no effect, because there is no
     * GL sharing here for it to synchronise with. */
    static constexpr uint64_t allowed[] = {
        static_cast<uint64_t>(CL_CONTEXT_PLATFORM),
        static_cast<uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC)};
    ClPropertyList            list;
    const cl_int              judged_list = judge(properties, allowed, 2u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list);
        return nullptr;
    }
    uint64_t named = 0u;
    if (list.names(static_cast<uint64_t>(CL_CONTEXT_PLATFORM), named) && named != 0u &&
        !platform_.names_this(reinterpret_cast<cl_platform_id>(named))) {
        report_error(error, CL_INVALID_PLATFORM);
        return nullptr;
    }
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
            case ClObject::Kind::command_queue: {
                /* Reads queued on it will never be waited for now, and their
                 * destinations are memory the application is about to reuse.
                 * The markers the ICD made for them go back with them. */
                uint64_t       markers[ClPendingReads::max_reads] = {};
                const uint32_t forgotten = reads_.forget_queue(wire_handle, markers);
                for (uint32_t index = 0u; index < forgotten; ++index)
                    static_cast<void>(av::clproto::rvk_clReleaseEvent(*wire, markers[index]));
                give_back_to_host(*wire, kind, wire_handle);
                break;
            }
            case ClObject::Kind::mem:
                give_back_to_host(*wire, kind, wire_handle);
                /* A buffer made over the arena gives its block back with it,
                 * so the next buffer can have that memory. After the host
                 * has let go, not before: the device's cl_mem is still over
                 * those bytes until it does. */
                if (held->backed()) context->blocks().give_back(held->backing_offset());
                break;
            default:
                give_back_to_host(*wire, kind, wire_handle);
                break;
            }
        }
    }
    const bool held_still = !last;
    static_cast<void>(objects_.release(*held));
    if (!held_still && kind == ClObject::Kind::context && context != nullptr) {
        /* Callbacks registered on this context's events are dropped first:
         * their transport is about to go, and asking the host over a torn
         * down one is what would happen otherwise. */
        watcher_.forget_context(context->wire());
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
    writer.number("reads_outstanding", reads_.outstanding());
    writer.number("reads_delivered", reads_.delivered());
    connection_.report(writer);
    writer.close();
}

bool ClIcdRoot::invariants_hold() const
{
    AV_SCOPE("ClIcdRoot::invariants_hold");
    if (!platform_.invariants_hold() || !extensions_.invariants_hold()) return false;
    if (!objects_.invariants_hold() || !connection_.invariants_hold()) return false;
    if (!device_.invariants_hold()) return false;
    if (!reads_.invariants_hold() || !watcher_.invariants_hold()) return false;
    /* A platform the loader would drop is one this ICD must never hand out. */
    return platform_.dispatchable();
}

cl_command_queue ClIcdRoot::create_queue(cl_context context, cl_device_id device,
                                         const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_queue");
    /* The two the API names for a queue. The bits CL_QUEUE_PROPERTIES may
     * carry are judged by the host, which is the only thing that knows which
     * of them its device supports. */
    static constexpr uint64_t allowed[] = {static_cast<uint64_t>(CL_QUEUE_PROPERTIES),
                                           static_cast<uint64_t>(CL_QUEUE_SIZE)};
    ClPropertyList            list;
    const cl_int              judged_list = judge(properties, allowed, 2u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list);
        return nullptr;
    }
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
                                void *host_pointer, const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_buffer");
    /* OpenCL 3.0 defines no buffer property, so a list with anything in it
     * names something this driver does not implement - which is the answer
     * the API asks for rather than one to ignore. */
    ClPropertyList list;
    const cl_int   judged_list = judge(properties, nullptr, 0u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list);
        return nullptr;
    }
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
    /* A buffer the application says it wants host-accessible is the one worth
     * the arena: CL_MEM_ALLOC_HOST_PTR is exactly the ask, and backing every
     * buffer there would put memory a kernel reads hardest in host memory
     * rather than the device's. Where the capset offered no host-visible
     * memory the blocks are not ready and every buffer is staged, which is
     * the path this had before.
     *
     * Not when the application brought its own pointer: USE_HOST_PTR and
     * COPY_HOST_PTR both name memory of its own, which is not the arena's. */
    GuestClContext *const on = held->context();
    const bool            wants_host_memory =
        (flags & static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR)) != 0u &&
        (flags & static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR)) == 0u;
    uint64_t block = 0u;
    bool     backed = false;
    if (wants_host_memory && on != nullptr && on->blocks().ready())
        backed = on->blocks().take(bytes, block);

    cl_int   host_error = CL_SUCCESS;
    uint64_t buffer = 0u;
    if (backed) {
        ClBlobBacking backing;
        backing.set(block);
        uint64_t named[4] = {};
        static_cast<void>(backing.write(named));
        buffer = av::clproto::rvk_clCreateBufferWithProperties(*wire, held->wire(), named, flags,
                                                               bytes, nullptr, &host_error);
        if (buffer == 0u) {
            /* The device would not make it over that memory, so the block is
             * given back and the ordinary buffer is asked for instead: a
             * staged buffer is slower and never wrong. */
            on->blocks().give_back(block);
            backed = false;
            host_error = CL_SUCCESS;
        }
    }
    if (buffer == 0u) {
        buffer = av::clproto::rvk_clCreateBuffer(*wire, held->wire(), flags, bytes, host_pointer,
                                                 &host_error);
    }
    if (buffer == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    void *const made = wrap(ClObject::Kind::mem, buffer, held->context(), held, error);
    if (made == nullptr) {
        /* wrap gave the host's buffer back, so only the block is left. */
        if (backed) on->blocks().give_back(block);
        return nullptr;
    }
    if (backed) static_cast<ClObject *>(made)->back_with(block, bytes);
    return static_cast<cl_mem>(made);
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
    /* A non-blocking read may not fetch here: the API says the application's
     * memory is undefined until the event completes, and the bytes do not
     * exist yet - the reply to a read the host has only queued carries
     * nothing. So what is sent is a marker, which is the read's place in the
     * queue as an event, and the fetch is recorded against it.
     *
     * The cost is that the bytes are the buffer's at delivery rather than at
     * enqueue. An application that writes to the buffer between the two sees
     * the later contents; the API lets it modify the buffer, so this is a
     * deviation, and it is the one the wire allows without the device
     * staging a copy of its own. */
    uint64_t     produced = 0u;
    if (blocking != CL_TRUE) {
        const cl_int marked = av::clproto::rvk_clEnqueueMarkerWithWaitList(
            *wire, held->wire(), 0u, nullptr, &produced);
        if (marked != CL_SUCCESS) return marked;
        /* An event the application did not ask for is one the ICD keeps
         * until the read delivers: it is the row's name, and releasing it
         * now would let the host hand the number to something else. */
        if (reads_.record(produced, held->wire(), named->wire(), offset, bytes, into,
                          event == nullptr)) {
            if (event != nullptr) {
                *event = static_cast<cl_event>(
                    wrap(ClObject::Kind::event, produced, held->context(), held, nullptr));
            }
            return CL_SUCCESS;
        }
        /* No room to remember it. The read still has to deliver, so it is
         * made blocking instead: slower, and never wrong. */
        static_cast<void>(av::clproto::rvk_clReleaseEvent(*wire, produced));
        log_.write("rvkcl: too many reads outstanding; this one blocks");
        produced = 0u;
    }
    const cl_int ran = av::clproto::rvk_clEnqueueReadBuffer(
        *wire, held->wire(), named->wire(), CL_FALSE, offset, bytes, into, 0u, nullptr, &produced);
    if (ran != CL_SUCCESS) return ran;
    const cl_int waited = waits_.until_queued_completes(*wire, held->wire(), produced);
    if (event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(ClObject::Kind::event, produced, held->context(), held, nullptr));
    } else {
        /* An application that wanted no event still made one here, and this
         * is the only place that can give it back. */
        static_cast<void>(av::clproto::rvk_clReleaseEvent(*wire, produced));
    }
    return waited;
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
    /* Enqueued without blocking whatever the application asked for, and with
     * an event whether or not it wanted one: the blocking half is this side's
     * wait on that event, so the host is never asked to hold its worker. */
    uint64_t     produced = 0u;
    const cl_int ran = av::clproto::rvk_clEnqueueWriteBuffer(
        *wire, held->wire(), named->wire(), CL_FALSE, offset, bytes, from, 0u, nullptr, &produced);
    if (ran != CL_SUCCESS) return ran;
    cl_int waited = CL_SUCCESS;
    if (blocking == CL_TRUE) waited = waits_.until_queued_completes(*wire, held->wire(), produced);
    if (event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(ClObject::Kind::event, produced, held->context(), held, nullptr));
    } else {
        /* An application that wanted no event still made one here, and this
         * is the only place that can give it back. */
        static_cast<void>(av::clproto::rvk_clReleaseEvent(*wire, produced));
    }
    return waited;
}

cl_int ClIcdRoot::finish(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::finish");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* Not the host's own clFinish: that blocks the device's worker for as
     * long as the queue takes, and a guest waiting on the fence past TdrDelay
     * has dxgkrnl reset the adapter. A marker is what the end of the queue
     * looks like as an event, and waiting for it holds nothing. */
    uint64_t     marker = 0u;
    const cl_int enqueued = av::clproto::rvk_clEnqueueMarkerWithWaitList(
        *wire, held->wire(), 0u, nullptr, &marker);
    if (enqueued != CL_SUCCESS) return enqueued;
    const cl_int waited = waits_.until_queued_completes(*wire, held->wire(), marker);
    static_cast<void>(av::clproto::rvk_clReleaseEvent(*wire, marker));
    if (waited != CL_SUCCESS) return waited;
    /* Everything queued before the marker has run, so every read outstanding
     * on this queue is one the application may now read. */
    return deliver_queued(*wire, held->wire());
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


void *ClIcdRoot::map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                            cl_map_flags flags, size_t offset, size_t bytes, cl_event *event,
                            cl_int *error)
{
    AV_SCOPE("ClIcdRoot::map_buffer");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) {
        report_error(error, CL_INVALID_COMMAND_QUEUE);
        return nullptr;
    }
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) {
        report_error(error, CL_INVALID_MEM_OBJECT);
        return nullptr;
    }
    if (bytes == 0u) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    GuestClContext *const context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    /* Where the buffer's memory is already the guest's, the map costs no copy
     * at all: the device made the host's cl_mem over these very bytes, so
     * what the application is handed is the arena's own view of them and the
     * unmap writes nothing back. The host is still asked to map, because its
     * map is what says the bytes are settled - a kernel may still be running
     * over them - but what it answers is a pointer this side already has.
     *
     * Bounds are re-checked here rather than trusted from the create: the
     * offset in the row is this ICD's, but the length is the application's. */
    if (named->backed() && offset <= named->backing_bytes() &&
        bytes <= named->backing_bytes() - offset) {
        uint8_t *const shared =
            context->blocks().at(named->backing_offset(), named->backing_bytes());
        if (shared != nullptr) {
            uint64_t       settled = 0u;
            cl_int         settle_error = CL_SUCCESS;
            const uint64_t host_mapped = av::clproto::rvk_clEnqueueMapBuffer(
                *wire, held->wire(), named->wire(), blocking, flags, offset, bytes, 0u, nullptr,
                event != nullptr ? &settled : nullptr, &settle_error);
            if (host_mapped == 0u) {
                report_error(error, settle_error != CL_SUCCESS ? settle_error : CL_MAP_FAILURE);
                return nullptr;
            }
            uint8_t *const at = shared + offset;
            /* Remembered like any other mapping, and with the host's pointer,
             * because the unmap names that one; what makes it the zero-copy
             * kind is that its memory is the arena's rather than a block of
             * its own, so nothing is read in and nothing written out. */
            if (context->take_shared_mapping(named->wire(), host_mapped, offset, bytes, at) ==
                nullptr) {
                report_error(error, CL_OUT_OF_HOST_MEMORY);
                return nullptr;
            }
            if (event != nullptr) {
                *event = static_cast<cl_event>(
                    wrap(ClObject::Kind::event, settled, context, held, nullptr));
            }
            report_error(error, CL_SUCCESS);
            return at;
        }
    }
    /* Staged: the guest gets a block of its own, filled from the buffer. The
     * host's own map is what says the bytes are settled, so it is asked
     * first. */
    uint64_t     mapped_event = 0u;
    cl_int       host_error = CL_SUCCESS;
    const uint64_t mapped = av::clproto::rvk_clEnqueueMapBuffer(
        *wire, held->wire(), named->wire(), blocking, flags, offset, bytes, 0u, nullptr,
        event != nullptr ? &mapped_event : nullptr, &host_error);
    if (mapped == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_MAP_FAILURE);
        return nullptr;
    }
    uint8_t *const at = context->take_mapping(named->wire(), mapped, offset, bytes, flags);
    if (at == nullptr) {
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    /* What the application is entitled to read: the buffer's own bytes. A
     * write-invalidate map promises nothing about them, so it is not read. */
    if ((flags & CL_MAP_WRITE_INVALIDATE_REGION) == 0u) {
        const cl_int read = av::clproto::rvk_clEnqueueReadBuffer(
            *wire, held->wire(), named->wire(), CL_TRUE, offset, bytes, at, 0u, nullptr, nullptr);
        if (read != CL_SUCCESS) {
            MappedRegion *const region = context->mapping_at(at);
            if (region != nullptr) context->give_back_mapping(*region);
            report_error(error, read);
            return nullptr;
        }
    }
    if (event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(ClObject::Kind::event, mapped_event, context, held, nullptr));
    }
    report_error(error, CL_SUCCESS);
    return at;
}

cl_int ClIcdRoot::unmap(cl_command_queue queue, cl_mem buffer, void *pointer, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::unmap");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    /* The pointer the map answered, and not an address inside it. */
    MappedRegion *const region = context->mapping_at(pointer);
    if (region == nullptr || region->buffer() != named->wire()) return CL_INVALID_VALUE;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     produced = 0u;
    const cl_int unmapped = av::clproto::rvk_clEnqueueUnmapMemObject(
        *wire, held->wire(), named->wire(), region->host(), 0u, nullptr,
        event != nullptr ? &produced : nullptr);
    /* The write-back follows the unmap, and not the other way round. A host
     * that cannot hand a guest its buffer maps by staging a copy, and its
     * unmap copies that staging area over the buffer - so bytes written
     * before the unmap are exactly the bytes the unmap discards. The mirror
     * is the application's own writes, so it goes last and wins. */
    cl_int written = CL_SUCCESS;
    if (unmapped == CL_SUCCESS && region->writes_back()) {
        written = av::clproto::rvk_clEnqueueWriteBuffer(*wire, held->wire(), named->wire(), CL_TRUE,
                                                        region->offset(), region->bytes(),
                                                        region->at(), 0u, nullptr, nullptr);
    }
    context->give_back_mapping(*region);
    if (unmapped != CL_SUCCESS) return unmapped;
    if (written != CL_SUCCESS) return written;
    if (event != nullptr) {
        *event = static_cast<cl_event>(wrap(ClObject::Kind::event, produced, context, held, nullptr));
    }
    return CL_SUCCESS;
}

void *ClIcdRoot::svm_alloc(cl_context context, cl_svm_mem_flags flags, size_t bytes,
                           cl_uint alignment)
{
    AV_SCOPE("ClIcdRoot::svm_alloc");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr || bytes == 0u) return nullptr;
    GuestClContext *const owner = held->context();
    ClientTransport *const wire = owner != nullptr ? owner->wire() : nullptr;
    if (wire == nullptr) return nullptr;
    const uint64_t host = av::clproto::rvk_clSVMAlloc(*wire, held->wire(), flags, bytes, alignment);
    if (host == 0u) return nullptr;
    /* The application is handed the mirror, because it is the pointer the
     * application dereferences; the host address is what crosses the wire. */
    void *const mirror = owner->svm().take(host, bytes);
    if (mirror == nullptr) {
        av::clproto::rvk_clSVMFree(*wire, held->wire(), host);
        return nullptr;
    }
    return mirror;
}

void ClIcdRoot::svm_free(cl_context context, void *pointer)
{
    AV_SCOPE("ClIcdRoot::svm_free");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr || pointer == nullptr) return;
    GuestClContext *const owner = held->context();
    if (owner == nullptr) return;
    /* Only the base the application was handed frees anything: an address
     * inside an allocation is not what clSVMAlloc answered. */
    const uint64_t host = owner->svm().give_back(pointer);
    if (host == 0u) return;
    ClientTransport *const wire = owner->wire();
    if (wire != nullptr) av::clproto::rvk_clSVMFree(*wire, held->wire(), host);
}

cl_int ClIcdRoot::svm_map(cl_command_queue queue, cl_bool blocking, cl_map_flags flags,
                          void *pointer, size_t bytes, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::svm_map");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    SvmAllocation *const allocation = context->svm().covering(pointer);
    if (allocation == nullptr || bytes == 0u) return CL_INVALID_VALUE;
    const uint64_t host = context->svm().host_address_of(pointer);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr || host == 0u) return CL_OUT_OF_RESOURCES;
    uint64_t     produced = 0u;
    const cl_int mapped = av::clproto::rvk_clEnqueueSVMMap(
        *wire, held->wire(), blocking, flags, host, bytes, 0u, nullptr,
        event != nullptr ? &produced : nullptr);
    if (mapped != CL_SUCCESS) return mapped;
    /* The host's map is what makes the allocation coherent on its side, and
     * this is where the mirror would be filled from it - except that no
     * command on this wire carries bytes between a host SVM address and guest
     * memory. clEnqueueSVMMemcpy names two SVM addresses, both the host's;
     * read and write carry bytes but only for a cl_mem. So a kernel's view of
     * shared memory is whole, and the host's view of it between map and
     * unmap is not yet: what an application writes into the mirror does not
     * reach the allocation (research §7.3).
     *
     * Said once, into the log, rather than pretended (constitution VI). */
    if (allocation->map_depth() == 0u) {
        log_.write("rvkcl: shared memory is mapped, and the mirror is not synchronised yet");
    }
    allocation->mapped();
    if (event != nullptr) {
        *event = static_cast<cl_event>(wrap(ClObject::Kind::event, produced, context, held, nullptr));
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::svm_unmap(cl_command_queue queue, void *pointer, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::svm_unmap");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    SvmAllocation *const allocation = context->svm().covering(pointer);
    if (allocation == nullptr) return CL_INVALID_VALUE;
    const uint64_t host = context->svm().host_address_of(pointer);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr || host == 0u) return CL_OUT_OF_RESOURCES;
    /* Where what the application wrote would go back; see svm_map for why it
     * does not yet. */
    uint64_t     produced = 0u;
    const cl_int unmapped = av::clproto::rvk_clEnqueueSVMUnmap(
        *wire, held->wire(), host, 0u, nullptr, event != nullptr ? &produced : nullptr);
    if (unmapped == CL_SUCCESS) {
        static_cast<void>(allocation->unmapped());
        if (event != nullptr) {
            *event = static_cast<cl_event>(
                wrap(ClObject::Kind::event, produced, context, held, nullptr));
        }
    }
    return unmapped;
}

cl_int ClIcdRoot::set_kernel_svm_argument(cl_kernel kernel, cl_uint index, const void *pointer)
{
    AV_SCOPE("ClIcdRoot::set_kernel_svm_argument");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    /* An address inside a live allocation, which a kernel argument is allowed
     * to be; one inside none is the application's mistake and never reaches
     * the wire. */
    const uint64_t host = context->svm().host_address_of(pointer);
    if (host == 0u) return CL_INVALID_ARG_VALUE;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::rvk_clSetKernelArgSVMPointer(*wire, held->wire(), index, host);
}


cl_event ClIcdRoot::create_user_event(cl_context context, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_user_event");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t event = av::clproto::rvk_clCreateUserEvent(*wire, held->wire(), &host_error);
    if (event == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_event>(wrap(ClObject::Kind::event, event, held->context(), held, error));
}

cl_int ClIcdRoot::set_user_event_status(cl_event event, cl_int status)
{
    AV_SCOPE("ClIcdRoot::set_user_event_status");
    ClObject *const held = judged(event, ClObject::Kind::event);
    if (held == nullptr) return CL_INVALID_EVENT;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::rvk_clSetUserEventStatus(*wire, held->wire(), status);
}

cl_int ClIcdRoot::wait_for_events(cl_uint count, const cl_event *events)
{
    AV_SCOPE("ClIcdRoot::wait_for_events");
    if (count == 0u || events == nullptr) return CL_INVALID_VALUE;
    /* Every event of one wait belongs to one context, because that is where a
     * transport is; the API says as much (CL_INVALID_CONTEXT otherwise). */
    uint64_t         handles[max_waited] = {};
    GuestClContext  *context = nullptr;
    const cl_uint    waited = count < max_waited ? count : max_waited;
    for (cl_uint index = 0u; index < waited; ++index) {
        ClObject *const held = judged(events[index], ClObject::Kind::event);
        if (held == nullptr) return CL_INVALID_EVENT;
        if (context == nullptr) context = held->context();
        if (held->context() != context) return CL_INVALID_CONTEXT;
        handles[index] = held->wire();
    }
    if (count > max_waited) return CL_OUT_OF_RESOURCES;
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* clWaitForEvents has no timeout, so the wait is unbounded - but every
     * host call it makes answers at once, which is the whole difference
     * between an unbounded wait and a held worker. */
    const cl_int reached = waits_.until_complete(*wire, handles, waited, icd::SlicedWait::forever);
    /* And a wait is where an application learns a non-blocking read is done,
     * so it is where the bytes have to be there. */
    const cl_int delivered = deliver_reads(*wire, handles, waited);
    return reached != CL_SUCCESS ? reached : delivered;
}

cl_int ClIcdRoot::event_info(cl_event event, cl_event_info name, size_t value_bytes, void *value,
                             size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::event_info");
    ClObject *const held = judged(event, ClObject::Kind::event);
    if (held == nullptr) return CL_INVALID_EVENT;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int asked =
        av::clproto::rvk_clGetEventInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
    /* An application that polls rather than waits learns of the completion
     * here, so this is where the bytes have to be there too. */
    if (asked == CL_SUCCESS && name == CL_EVENT_COMMAND_EXECUTION_STATUS && value != nullptr &&
        value_bytes >= sizeof(cl_int) && *static_cast<const cl_int *>(value) == CL_COMPLETE) {
        const uint64_t handle = held->wire();
        static_cast<void>(deliver_reads(*wire, &handle, 1u));
    }
    return asked;
}

cl_int ClIcdRoot::watch_event(cl_event event, cl_int status, ClWatchedEvent::Notify notify,
                              void *user_data)
{
    AV_SCOPE("ClIcdRoot::watch_event");
    ClObject *const held = judged(event, ClObject::Kind::event);
    if (held == nullptr) return CL_INVALID_EVENT;
    GuestClContext *const context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* The device registers its own callback with the host; no guest function
     * pointer crosses. What comes back is a wake, and the watcher is what
     * asks which events it was about. */
    const cl_int told = av::clproto::rvk_clSetEventCallback(*wire, held->wire(), status);
    if (told != CL_SUCCESS) return told;
    /* The application may release this event on its next line and the
     * callback still has to run and still has to be handed it, so the
     * registration gets a reference of its own. */
    lock_.acquire();
    held->retain();
    lock_.release();
    const cl_int registered = watcher_.watch(held, held->wire(), wire, status, notify, user_data);
    if (registered != CL_SUCCESS) static_cast<void>(release(held, ClObject::Kind::event));
    return registered;
}


cl_int ClIcdRoot::deliver_reads(ClientTransport &wire, const uint64_t *events, uint32_t count)
{
    AV_SCOPE("ClIcdRoot::deliver_reads");
    cl_int answer = CL_SUCCESS;
    for (uint32_t index = 0u; index < count; ++index) {
        ClPendingRead *const waiting = reads_.waiting_on(events[index]);
        if (waiting == nullptr) continue;
        /* The fetch the enqueue deferred. The host has reached the marker, so
         * everything queued before the read has run and this answers at once;
         * it is sent blocking because there is nothing left to wait for. */
        const cl_int read = av::clproto::rvk_clEnqueueReadBuffer(
            wire, waiting->queue(), waiting->buffer(), CL_TRUE, waiting->offset(),
            waiting->bytes(), waiting->into(), 0u, nullptr, nullptr);
        if (waiting->ours())
            static_cast<void>(av::clproto::rvk_clReleaseEvent(wire, waiting->event()));
        waiting->give_up();
        reads_.count_delivery();
        if (read != CL_SUCCESS && answer == CL_SUCCESS) answer = read;
    }
    return answer;
}

cl_int ClIcdRoot::deliver_queued(ClientTransport &wire, uint64_t queue)
{
    AV_SCOPE("ClIcdRoot::deliver_queued");
    cl_int answer = CL_SUCCESS;
    /* One pass per read rather than one over the table: delivering gives a
     * row up, so the table is walked again each time rather than while it
     * changes underneath. */
    for (uint32_t taken = 0u; taken < ClPendingReads::max_reads; ++taken) {
        uint64_t             event = 0u;
        for (uint32_t slot = 0u; slot < ClPendingReads::max_reads && event == 0u; ++slot) {
            const ClPendingRead *const row = reads_.at(slot);
            if (row->held() && row->queue() == queue) event = row->event();
        }
        if (event == 0u) break;
        const cl_int one = deliver_reads(wire, &event, 1u);
        if (one != CL_SUCCESS && answer == CL_SUCCESS) answer = one;
    }
    return answer;
}

void ClIcdRoot::deliver_watched(ClObject &event)
{
    AV_SCOPE("ClIcdRoot::deliver_watched");
    ClientTransport *const wire = wire_of(event);
    if (wire == nullptr) return;
    const uint64_t handle = event.wire();
    static_cast<void>(deliver_reads(*wire, &handle, 1u));
}

void ClIcdRoot::release_watched(ClObject &event)
{
    AV_SCOPE("ClIcdRoot::release_watched");
    static_cast<void>(release(&event, ClObject::Kind::event));
}

}  // namespace av::clicd
