#include "icd/opencl/cl_icd_root.hpp"

#include "icd/opencl/cl_extension_policy.hpp"

#include "icd/opencl/d3d11_bridge.hpp"
#include "icd/opencl/d3d11_object.hpp"

#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "guest/kmt/pci_location.hpp"
#include "icd/common/adapter_string.hpp"

#include "icd/opencl/cl_info_request.hpp"

#include "proto/resources/cl_blob_backing.hpp"

#include "icd/common/report_gate.hpp"
#include "icd/common/sliced_wait.hpp"

#include "core/support/scope_exit.hpp"
#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "core/log/log_sink.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/opencl/unanswered_transport.hpp"
#include "icd/opencl/cl_external_memory.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "icd/opencl/d3d_adapter_source.hpp"
#include "icd/opencl/gl_share_source.hpp"
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

/* The unit CL_DEVICE_GLOBAL_FREE_MEMORY_AMD answers in. */
constexpr uint64_t kib = 1024u;

}  // namespace

ClIcdRoot::ClIcdRoot(BlockSource &memory, Kmt &kmt, icd::ThreadLock &lock,
                     icd::ThreadLock &send_lock, icd::ThreadLock &objects_lock, icd::ThreadLock &blocks_lock,
                     LogSink &log,
                     icd::ServiceThread &watcher_thread, GlShareSource &gl_share, D3dAdapterSource &d3d_adapters,
                     D3d11Bridge &d3d11,
                     ClIcdBinding &binding)
    : lock_(lock), log_(log), gl_share_(gl_share), d3d_adapters_(d3d_adapters), d3d11_(d3d11), binding_(binding), dispatch_(), platform_(),
      objects_(memory, dispatch_.table(), objects_lock), kmt_(kmt), devices_(kmt),
      shared_handles_(kmt, lock, memory), contexts_(memory), memory_(memory), send_lock_(send_lock), blocks_lock_(blocks_lock),
      watcher_(watcher_thread, log, lock, *this), waits_(kmt, watcher_), device_answers_(), reads_(), refusals_(log),
      destructor_callbacks_()
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
        /* What every device of the platform carries: the extensions this ICD
         * answers alone, whatever host or adapter is under it - asked of a
         * policy that has seen neither, before any device exists, as the
         * loader asks while enumerating. DaVinci Resolve looks here for
         * cl_khr_d3d10_sharing before it asks which device a DXGI adapter is,
         * and found no OpenCL GPU while this was empty. */
        const ClExtensionPolicy platform_wide(reported_extensions, reported_extension_count);
        static const char no_host[] = "";
        const size_t needed = platform_wide.report(no_host, sizeof no_host, nullptr, 0u);
        if (value != nullptr) {
            if (value_bytes < needed) return CL_INVALID_VALUE;
            static_cast<void>(platform_wide.report(no_host, sizeof no_host, static_cast<char *>(value), value_bytes));
        }
        if (size_ret != nullptr) *size_ret = needed;
        return CL_SUCCESS;
    }
    if (name == CL_PLATFORM_EXTENSIONS_WITH_VERSION) {
        if (!platform_.names_this(platform)) return CL_INVALID_PLATFORM;
        return platform_extension_versions(value_bytes, value, size_ret);
    }
    /* Asked before any device exists, the name is found by the adapters'
     * handles alone - no device is opened for it, which every process asking
     * would then pay. */
    if (name == CL_PLATFORM_NAME) {
        lock_.acquire();
        learn_devices();
        lock_.release();
    }
    return platform_.info(platform, name, value_bytes, value, size_ret);
}

void ClIcdRoot::learn_devices()
{
    AV_SCOPE("ClIcdRoot::learn_devices");
    /* A lone device Windows would not describe is asked about again: its name
     * is the platform's, and an application shows the platform's name. */
    if (devices_.learnt() && (devices_.count() != 1u || platform_.names_an_adapter())) return;
    AvgpuAdapters adapters(kmt_);
    if (adapters.learn() != Status::ok) return;
    if (!devices_.learnt()) {
        devices_.learn(adapters);
        if (devices_.left_out() != 0u)
            log_.write("avcl: an avgpu adapter is left out: no LUID, or past the devices shown");
    }
    icd::AdapterString name;
    for (uint32_t index = 0u; index < devices_.count(); ++index) {
        ClDevice &device = devices_.at(index);
        describe_adapter(adapters.adapter(adapters.index_of(device.luid())), device, name);
    }
    /* The platform is named after its GPU when it has one; with several, no
     * one GPU's name says what it is. */
    if (devices_.count() == 1u) platform_.observe_adapter(name.text());
}

/* What Windows says of the adapter the device is on, which is what the
 * device's vendor, name and PCI location are (docs/vendor-libraries.md): an
 * adapter it will not describe leaves those as they were. */
void ClIcdRoot::describe_adapter(uint32_t adapter, ClDevice &device, icd::AdapterString &name)
{
    AV_SCOPE("ClIcdRoot::describe_adapter");
    PciIdentity identity;
    PciLocation location;
    uint16_t    wide[icd::AdapterString::bytes] = {};
    if (kmt_.query_identity(adapter, identity) != Status::ok ||
        kmt_.query_adapter_location(adapter, location) != Status::ok ||
        kmt_.query_adapter_name(adapter, wide, icd::AdapterString::bytes) != Status::ok)
        return;
    device.decided().observe_adapter(identity, location, wide);
    device.extensions().observe_vendor(identity.vendor());
    name.take(wide);
}

bool ClIcdRoot::spells(const char *name, const char *wanted) const
{
    AV_SCOPE("ClIcdRoot::spells");
    size_t index = 0u;
    while (wanted[index] != '\0' && name[index] == wanted[index]) ++index;
    return wanted[index] == '\0' && name[index] == '\0';
}

void *ClIcdRoot::extension_function(const char *name)
{
    AV_SCOPE("ClIcdRoot::extension_function");
    if (name == nullptr) return nullptr;
    /* The three names the loader asks for rather than dispatching through a
     * slot: the ICD's own platform enumeration, which the ABI has no slot
     * for, and `cl_khr_external_memory`'s two, which the ABI predates. */
    if (spells(name, "clIcdGetPlatformIDsKHR")) return dispatch_.icd_get_platform_ids();
    if (spells(name, "clEnqueueAcquireExternalMemObjectsKHR"))
        return dispatch_.external_mem_acquire();
    if (spells(name, "clEnqueueReleaseExternalMemObjectsKHR"))
        return dispatch_.external_mem_release();
    if (spells(name, "clGetGLContextInfoKHR")) return dispatch_.gl_context_info();
    if (spells(name, "clCreateProgramWithILKHR")) return dispatch_.program_from_il();
    return nullptr;
}

Status ClIcdRoot::open_device(ClDevice &device)
{
    AV_SCOPE("ClIcdRoot::open_device");
    if (device.object() != nullptr) return Status::ok;
    /* The ICD's own context: what its device queries are asked over, and what
     * holds the adapter open for as long as a device exists. An application's
     * contexts are its own, made beside this one. */
    GuestClContext *context = nullptr;
    const Status    made =
        contexts_.create(context, device.connection(), memory_, send_lock_, blocks_lock_, log_, shared_handles_);
    if (made != Status::ok) {
        log_.write("avcl: no room for the ICD's own context");
        return made;
    }
    /* A context that was made has a transport - that is what being made
     * means - so the two are one question and one failure path. */
    const Status           opened = context->make();
    ClientTransport *const wire = opened == Status::ok ? context->wire() : nullptr;
    if (wire == nullptr) {
        log_.write("avcl: the ICD's own context could not be made");
        static_cast<void>(contexts_.destroy(context));
        /* Only a make that failed gets here, since one that did not leaves a
         * transport behind it. */
        return opened;
    }
    /* What the host has, asked once. The device it answers is the one this
     * device stands for, because the executor bound one GPU to the adapter. */
    uint64_t host_platform = 0u;
    cl_uint  platforms = 0u;
    if (av::clproto::av_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) != CL_SUCCESS ||
        platforms == 0u || host_platform == 0u) {
        static_cast<void>(contexts_.destroy(context));
        return Status::not_supported;
    }
    uint64_t host_device = 0u;
    cl_uint  devices = 0u;
    if (av::clproto::av_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device,
                                        &devices) != CL_SUCCESS ||
        devices == 0u || host_device == 0u) {
        static_cast<void>(contexts_.destroy(context));
        return Status::not_supported;
    }
    ClObject *object = nullptr;
    const Status held = objects_.make(ClObject::Kind::device, host_device, context, nullptr, object);
    if (held != Status::ok) {
        static_cast<void>(contexts_.destroy(context));
        return held;
    }
    device.opened(*object);
    /* What the device says about itself, capped at what this ICD implements. */
    char text[64] = {};
    size_t bytes = 0u;
    if (av::clproto::av_clGetDeviceInfo(*wire, host_device, CL_DEVICE_VERSION, sizeof text, text,
                                         &bytes) == CL_SUCCESS) {
        static_cast<void>(device.decided().observe_version(text, bytes));
    }
    if (av::clproto::av_clGetDeviceInfo(*wire, host_device, CL_DEVICE_OPENCL_C_VERSION, sizeof text,
                                         text, &bytes) == CL_SUCCESS) {
        static_cast<void>(device.decided().observe_c_version(text, bytes));
    }
    cl_device_svm_capabilities svm = 0u;
    if (av::clproto::av_clGetDeviceInfo(*wire, host_device, CL_DEVICE_SVM_CAPABILITIES, sizeof svm,
                                         &svm, &bytes) == CL_SUCCESS) {
        device.decided().observe_svm(svm);
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
    /* Every device whose adapter opens, in their order: the count is what
     * there is, and the list as many of them as fit. */
    cl_uint found = 0u;
    lock_.acquire();
    learn_devices();
    for (uint32_t index = 0u; index < devices_.count(); ++index) {
        ClDevice &device = devices_.at(index);
        if (open_device(device) != Status::ok) continue;
        if (devices != nullptr && found < num_entries) devices[found] = reinterpret_cast<cl_device_id>(device.object());
        ++found;
    }
    lock_.release();
    if (num_devices != nullptr) *num_devices = found;
    return found != 0u ? CL_SUCCESS : CL_DEVICE_NOT_FOUND;
}

uint64_t ClIcdRoot::host_device_of(ClObject &object)
{
    AV_SCOPE("ClIcdRoot::host_device_of");
    GuestClContext *const context = context_of(object);
    ClDevice *const       device = context != nullptr ? devices_.running(*context) : nullptr;
    return device != nullptr && device->object() != nullptr ? device->object()->wire() : 0u;
}

cl_int ClIcdRoot::device_info(cl_device_id device, cl_device_info name, size_t value_bytes,
                              void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::device_info");
    ClObject *const object = judged(device, ClObject::Kind::device);
    ClDevice *const shown = object != nullptr ? devices_.of(*object) : nullptr;
    if (shown == nullptr) return CL_INVALID_DEVICE;
    /* The four this ICD decides name the avgpu adapter rather than the host
     * GPU, or cap what it implements; everything else is the host's own. */
    if (shown->decided().decides(name)) return shown->decided().info(name, value_bytes, value, size_ret);
    /* The answers that are objects: the host's handles for its platform and
     * its parent device are nothing in this process - an application hands
     * the platform straight back, to clGetDeviceIDsFromD3D10KHR among others,
     * through a loader that dereferences it. */
    if (name == CL_DEVICE_PLATFORM) {
        const cl_platform_id ours = platform_.as_handle();
        return ClInfoRequest(value_bytes, value, size_ret).give(&ours, sizeof ours);
    }
    if (name == CL_DEVICE_PARENT_DEVICE) {
        const cl_device_id none = nullptr;
        return ClInfoRequest(value_bytes, value, size_ret).give(&none, sizeof none);
    }
    /* Except the extensions, which are neither: the host's list is what the
     * GPU has and the policy is what this ICD carries, and passing the host's
     * through would offer an application every extension rusticl implements
     * over a wire that carries none of them. */
    if (name == CL_DEVICE_EXTENSIONS) return device_extensions(*shown, value_bytes, value, size_ret);
    if (name == CL_DEVICE_EXTENSIONS_WITH_VERSION)
        return device_extension_versions(*shown, value_bytes, value, size_ret);
    if (name == CL_DEVICE_OPENCL_C_FEATURES) return c_features(*shown, value_bytes, value, size_ret);
    /* And the handle types an import may name, which is the question an
     * application asks before it names one: this driver takes the opaque
     * Win32 handle and nothing else, and answers so only where it reports the
     * extension that defines the query. */
    if (name == CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR)
        return import_handle_types(*shown, value_bytes, value, size_ret);
    if (name == free_memory_amd && shown->decided().on_vendor(ClDeviceReport::amd_vendor))
        return free_memory(*shown, value_bytes, value, size_ret);
    ClientTransport *const wire = wire_of(*object);
    if (wire == nullptr) return CL_INVALID_DEVICE;
    return host_device_info(*wire, *shown, name, value_bytes, value, size_ret);
}

/* Asked of the host with room for the longest answer kept, whatever the
 * caller's room, so the answer kept is whole. A query refused as an invalid
 * value - an unknown name, or an answer longer than that room - is asked again
 * as the caller put it, so the caller gets the host's own answer to it. */
cl_int ClIcdRoot::free_memory(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::free_memory");
    uint64_t budget = 0u;
    uint64_t usage = 0u;
    if (kmt_.query_video_memory(device.connection().adapter(), MemorySegmentGroup::local, budget, usage) !=
        Status::ok)
        return CL_OUT_OF_RESOURCES;
    /* Nothing says how the free memory is broken up, so the largest block is
     * all of it, as AMD's own driver answers on an idle GPU. */
    const size_t free_kib = static_cast<size_t>((budget > usage ? budget - usage : 0u) / kib);
    const size_t answer[2] = {free_kib, free_kib};
    return ClInfoRequest(value_bytes, value, size_ret).give(answer, sizeof answer);
}

cl_int ClIcdRoot::host_device_info(ClientTransport &wire, ClDevice &device, cl_device_info name,
                                   size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::host_device_info");
    const ClInfoRequest asked(value_bytes, value, size_ret);
    const uint64_t      host = device.object()->wire();
    /* Kept by the guest's device, which no two devices share: two host GPUs'
     * handles are numbers of two drivers' making, and may be equal. */
    const uint64_t      key = reinterpret_cast<uintptr_t>(device.object());
    cl_int              kept = CL_SUCCESS;
    if (device_answers_.answer(key, name, asked, kept)) return kept;
    uint8_t      answer[host_extensions_bytes];
    size_t       answered = 0u;
    const cl_int fetched = av::clproto::av_clGetDeviceInfo(wire, host, name, sizeof answer, answer, &answered);
    if (fetched == CL_INVALID_VALUE)
        return av::clproto::av_clGetDeviceInfo(wire, host, name, value_bytes, value, size_ret);
    if (fetched != CL_SUCCESS) return fetched;
    device_answers_.keep(key, name, answer, answered);
    return asked.give(answer, answered);
}

cl_int ClIcdRoot::device_extensions(ClDevice &device, size_t value_bytes, void *value,
                                    size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::device_extensions");
    ClientTransport *const wire = wire_of(*device.object());
    if (wire == nullptr) return CL_INVALID_DEVICE;
    char         host[host_extensions_bytes] = {};
    size_t       answered = 0u;
    const cl_int asked = host_device_info(*wire, device, CL_DEVICE_EXTENSIONS, sizeof host, host, &answered);
    if (asked != CL_SUCCESS) return asked;
    /* `answered` needs no clamp of its own: the generated client refuses a
     * size the device reports past the room it was given, so a device that
     * answered CL_SUCCESS answered a length this buffer holds. */
    /* Measured, then written: the policy writes nothing into room its answer
     * does not fit, which is what lets the size query and the fetch be the
     * same call twice. */
    const size_t needed = device.extensions().report(host, answered, nullptr, 0u);
    if (size_ret != nullptr) *size_ret = needed;
    if (value == nullptr) return CL_SUCCESS;
    if (value_bytes < needed) return CL_INVALID_VALUE;
    static_cast<void>(device.extensions().report(host, answered, static_cast<char *>(value), value_bytes));
    return CL_SUCCESS;
}

cl_int ClIcdRoot::import_handle_types(ClDevice &device, size_t value_bytes, void *value,
                                      size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::import_handle_types");
    char         reported[host_extensions_bytes] = {};
    const cl_int asked = device_extensions(device, sizeof reported, reported, nullptr);
    if (asked != CL_SUCCESS) return asked;
    /* A query of an extension this device does not report is a query it does
     * not have, which is what the API answers CL_INVALID_VALUE for. */
    if (!device.extensions().offered_by(reported, sizeof reported, "cl_khr_external_memory"))
        return CL_INVALID_VALUE;
    const cl_external_memory_handle_type_khr taken = CL_EXTERNAL_MEMORY_HANDLE_OPAQUE_WIN32_KHR;
    if (size_ret != nullptr) *size_ret = sizeof taken;
    if (value == nullptr) return CL_SUCCESS;
    if (value_bytes < sizeof taken) return CL_INVALID_VALUE;
    *static_cast<cl_external_memory_handle_type_khr *>(value) = taken;
    return CL_SUCCESS;
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
        static_cast<void>(av::clproto::av_clReleaseCommandQueue(wire, handle));
        break;
    case ClObject::Kind::mem:
        static_cast<void>(av::clproto::av_clReleaseMemObject(wire, handle));
        break;
    case ClObject::Kind::program:
        static_cast<void>(av::clproto::av_clReleaseProgram(wire, handle));
        break;
    case ClObject::Kind::kernel:
        static_cast<void>(av::clproto::av_clReleaseKernel(wire, handle));
        break;
    case ClObject::Kind::event:
        static_cast<void>(av::clproto::av_clReleaseEvent(wire, handle));
        break;
    case ClObject::Kind::sampler:
        static_cast<void>(av::clproto::av_clReleaseSampler(wire, handle));
        break;
    /* A context is torn down rather than released through here, and a
     * platform and a device are never the host's to give back. */
    default: break;
    }
}

void *ClIcdRoot::wrap(ClientTransport &transport, ClObject::Kind kind, uint64_t wire,
                      GuestClContext *context, ClObject *parent, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::wrap");
    ClObject *object = nullptr;
    if (objects_.make(kind, wire, context, parent, object) != Status::ok) {
        /* The host made it and the guest cannot keep it, so nobody will ever
         * name it again. Giving it back here is the only chance: the
         * application is about to be told the call failed, and an object it
         * was never handed is one it can never release. */
        give_back_to_host(transport, kind, wire);
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    /* Its place on its queue, which the watcher asks in. */
    if (kind == ClObject::Kind::event && parent != nullptr && parent->kind() == ClObject::Kind::command_queue)
        object->stamp(parent->take_ordinal());
    report_error(error, CL_SUCCESS);
    return object;
}

template <typename Property>
cl_int ClIcdRoot::judge(const Property *properties, const uint64_t *allowed, uint32_t allowed_count,
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
        if (known) continue;
        /* Named, because an application that only checks for failure - an
         * OpenCL capability probe - otherwise leaves nothing to say why. */
        char       bytes[80];
        TextBuffer line(bytes, sizeof bytes);
        line.append("avcl: a property this ICD does not take: 0x");
        line.append_hex(name, 4u);
        log_.write(line.text());
        return CL_INVALID_PROPERTY;
    }
    return CL_SUCCESS;
}

cl_context ClIcdRoot::create_context(const cl_context_properties *properties, const cl_device_id *devices,
                                     cl_uint num_devices, cl_int *error, const ClContextNotify &notify)
{
    AV_SCOPE("ClIcdRoot::create_context");
    /* CL_CONTEXT_PLATFORM must be this platform if it is named at all. The
     * interop-sync flag is accepted and has no effect: a GL-sharing context's
     * application finishes its GL work before it acquires (the extension's
     * own rule), and the host is told nothing it would change. */
    static constexpr uint64_t allowed[] = {
        static_cast<uint64_t>(CL_CONTEXT_PLATFORM), static_cast<uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC),
        static_cast<uint64_t>(CL_GL_CONTEXT_KHR), static_cast<uint64_t>(CL_WGL_HDC_KHR),
        static_cast<uint64_t>(d3d10_context_device), static_cast<uint64_t>(d3d11_context_device)};
    ClPropertyList            list;
    const cl_int              judged_list = judge(properties, allowed, 6u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list);
        return nullptr;
    }
    /* A Direct3D 10 device to share with is one this device cannot share
     * with: nothing crosses that could name the device's surfaces. A null
     * one names no device, which the extension allows. */
    uint64_t d3d10_device = 0u;
    if (list.names(static_cast<uint64_t>(d3d10_context_device), d3d10_device) && d3d10_device != 0u) {
        report_error(error, invalid_d3d10_device);
        return nullptr;
    }
    uint32_t     gl_share = 0u;
    const cl_int shared = gl_share_for(list, gl_share);
    if (shared != CL_SUCCESS) {
        report_error(error, shared);
        return nullptr;
    }
    /* A context may share with OpenGL and Direct3D 11 both. The extensions
     * say CL_INVALID_OPERATION, and AMD's and NVIDIA's drivers make the
     * context anyway - DaVinci Resolve asks for exactly that, and gets no GPU
     * when refused. The two never meet: GL objects are the host's share
     * group's, and Direct3D 11 ones are copied through the device. */
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
    /* One device, and it has to be this ICD's: devices on two adapters run on
     * two host GPUs, which no one host context spans. */
    ClObject *const named_device = num_devices == 1u ? judged(devices[0], ClObject::Kind::device) : nullptr;
    ClDevice *const device = named_device != nullptr ? devices_.of(*named_device) : nullptr;
    if (device == nullptr) {
        report_error(error, CL_INVALID_DEVICE);
        return nullptr;
    }
    void        *d3d11_device = nullptr;
    const cl_int d3d11_shared = d3d11_device_for(list, *device, d3d11_device);
    if (d3d11_shared != CL_SUCCESS) {
        report_error(error, d3d11_shared);
        return nullptr;
    }
    lock_.acquire();
    GuestClContext *context = nullptr;
    Status          made =
        contexts_.create(context, device->connection(), memory_, send_lock_, blocks_lock_, log_, shared_handles_);
    if (made == Status::ok) made = context->make(gl_share, watcher_.wake_handle());
    if (made == Status::ok) context->keep_properties(properties);
    if (made == Status::ok) context->notify_with(notify);
    if (made != Status::ok) {
        if (context != nullptr) static_cast<void>(contexts_.destroy(context));
        lock_.release();
        /* A context the device would not put in that GL context's share group
         * is the extension's refusal, not a missing device. */
        report_error(error, made == Status::no_memory ? CL_OUT_OF_HOST_MEMORY
                            : gl_share != 0u          ? CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR
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
    /* The make above is what says the transport is there. */
    if (av::clproto::av_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) == CL_SUCCESS &&
        av::clproto::av_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device,
                                        &found) == CL_SUCCESS) {
        host_context = av::clproto::av_clCreateContext(*wire, nullptr, 1u, &host_device,
                                                        &host_error);
    }
    if (host_context == 0u) {
        static_cast<void>(contexts_.destroy(context));
        lock_.release();
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_DEVICE_NOT_AVAILABLE);
        return nullptr;
    }
    void *const wrapped = wrap(*wire, ClObject::Kind::context, host_context, context, nullptr, error);
    if (wrapped == nullptr) static_cast<void>(contexts_.destroy(context));
    /* Held for the context's life: every create and copy on it uses it. */
    if (wrapped != nullptr && d3d11_device != nullptr) {
        context->d3d11().share_with(d3d11_device);
        d3d11_.hold(d3d11_device);
    }
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
    /* A sub-buffer's last reference gives back the one it held on its
     * buffer, once this one has gone and the lock is free again. */
    ClObject *const       sliced_from = last ? held->sliced_from() : nullptr;
    /* One over a Direct3D 11 resource lets go of the resource, which the
     * extension's reference is the cl_mem's: whether or not the device can
     * still be told, since the resource is this process's either way. */
    void *twin = nullptr;
    if (last && kind == ClObject::Kind::mem && context != nullptr) {
        const D3d11Object *const shared = context->d3d11().find(object);
        twin = shared != nullptr ? shared->twin() : nullptr;
        void *resource = nullptr;
        if (context->d3d11().forget(object, resource)) d3d11_.let_go(resource);
    }
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
                    static_cast<void>(av::clproto::av_clReleaseEvent(*wire, markers[index]));
                give_back_to_host(*wire, kind, wire_handle);
                break;
            }
            case ClObject::Kind::mem:
                /* A cl_mem the device made inside an open of a shared object
                 * is released rather than freed on the host: what goes is this
                 * context's reference on the object, and the device destroys
                 * what it made when the last one goes (feature 053). */
                /* Mappings it was never unmapped from go with it: nothing
                 * names them again, and each holds a row of the context. */
                refusals_.note_released_mapped(context->give_back_mappings_of(wire_handle));
                if (context->external().give_back(*wire, wire_handle)) break;
                give_back_to_host(*wire, kind, wire_handle);
                /* A buffer made over the arena gives its block back with it,
                 * so the next buffer can have that memory. After the host
                 * has let go, not before: the device's cl_mem is still over
                 * those bytes until it does. */
                if (held->backed() && held->sliced_from() == nullptr)
                    context->blocks().give_back(held->backing_offset());
                break;
            default:
                give_back_to_host(*wire, kind, wire_handle);
                break;
            }
        }
    }
    /* A twin goes after the image over it: the host's open of its memory is
     * given back first. */
    if (twin != nullptr) d3d11_.let_go(twin);
    /* The context goes when the last thing holding it does - the
     * application, or the last object made on it - which may be this release
     * of a kernel as well as of the context. */
    GuestClContext *freed = nullptr;
    const void     *gone[ClDestructorCallbacks::max_gone] = {};
    uint32_t        went = 0u;
    static_cast<void>(objects_.release(*held, freed, gone, ClDestructorCallbacks::max_gone, went));
    if (freed != nullptr) {
        /* Callbacks registered on this context's events are dropped first:
         * their transport is about to go, and asking the host over a torn
         * down one is what would happen otherwise. */
        watcher_.forget_context(freed->wire());
        if (freed->d3d11().device() != nullptr) d3d11_.let_go(freed->d3d11().device());
        /* Its objects are gone from the table; its transport and everything
         * the host held on it go now. */
        static_cast<void>(contexts_.destroy(freed));
    }
    ClDestructorCallbacks::Due due;
    destructor_callbacks_.take(gone, went, due);
    lock_.release();
    /* After the host let go and the context's transport went, as a driver
     * calls them once the object's memory is gone, and with no lock held: a
     * callback may release or make another object. */
    due.run();
    if (sliced_from != nullptr) static_cast<void>(release(sliced_from, ClObject::Kind::mem));
    return CL_SUCCESS;
}

void ClIcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("ClIcdRoot::report");
    /* Read under the root's lock, as every root's dump is: the trace thread
     * runs while application threads release objects and open devices. */
    const icd::ReportGate gate(lock_, writer, "clicd");
    if (!gate.held()) return;
    devices_.report(writer);
    objects_.report(writer);
    writer.number("reads_outstanding", reads_.outstanding());
    writer.number("reads_delivered", reads_.delivered());
    writer.number("wait_rounds", waits_.rounds());
    writer.number("wait_raises", waits_.raises_asked());
    writer.close();
}

bool ClIcdRoot::invariants_hold() const
{
    AV_SCOPE("ClIcdRoot::invariants_hold");
    if (!platform_.invariants_hold() || !objects_.invariants_hold() || !devices_.invariants_hold()) return false;
    if (!reads_.invariants_hold() || !watcher_.invariants_hold()) return false;
    /* A platform the loader would drop is one this ICD must never hand out. */
    return platform_.dispatchable();
}

cl_command_queue ClIcdRoot::create_queue(cl_context context, cl_device_id device,
                                         const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_queue");
    /* The two the API names for a queue. Of the bits CL_QUEUE_PROPERTIES may
     * carry, profiling is the one the host's queue is made with: a command's
     * timestamps are the host GPU's, and nothing here has a clock for them.
     * An out-of-order queue stays in order, which the API allows of any. */
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
    /* The device has to be the one the context is made on: a queue runs on
     * its context's host GPU and no other. */
    ClObject *const named_device = judged(device, ClObject::Kind::device);
    const uint64_t  host_device = host_device_of(*held);
    if (named_device == nullptr || host_device == 0u || named_device->wire() != host_device ||
        devices_.of(*named_device) != devices_.running(*held->context())) {
        report_error(error, CL_INVALID_DEVICE);
        return nullptr;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    uint64_t       bits = 0u;
    const bool     named = list.names(CL_QUEUE_PROPERTIES, bits);
    /* A device queue is one no device here has (ClDeviceReport), and a size
     * is a device queue's alone. */
    constexpr uint64_t known = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE | CL_QUEUE_PROFILING_ENABLE |
                               CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT;
    uint64_t           size = 0u;
    if ((bits & ~known) != 0u || list.names(CL_QUEUE_SIZE, size)) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    if ((bits & (CL_QUEUE_ON_DEVICE | CL_QUEUE_ON_DEVICE_DEFAULT)) != 0u) {
        report_error(error, CL_INVALID_QUEUE_PROPERTIES);
        return nullptr;
    }
    const uint64_t profiled[] = {static_cast<uint64_t>(CL_QUEUE_PROPERTIES), CL_QUEUE_PROFILING_ENABLE, 0u};
    cl_int         host_error = CL_SUCCESS;
    const uint64_t queue = av::clproto::av_clCreateCommandQueueWithProperties(
        *wire, held->wire(), host_device,
        named && (bits & CL_QUEUE_PROFILING_ENABLE) != 0u ? profiled : nullptr, &host_error);
    if (queue == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_command_queue>(
        wrap(*wire, ClObject::Kind::command_queue, queue, held->context(), held, error));
}

cl_mem ClIcdRoot::create_buffer(cl_context context, cl_mem_flags flags, size_t bytes,
                                void *host_pointer, const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_buffer");
    /* OpenCL 3.0 defines no buffer property of its own, so the only names a
     * list may carry are cl_khr_external_memory's five. They are let through
     * whether or not the row that carries them is reported for this device:
     * the four this path cannot take are refused with the reason by the table
     * that knows which they are, and the one it can take is refused by the
     * device until its OpenCL backend can import (T019). Reporting the row is
     * a separate question, and the answer is still no. */
    ClPropertyList list;
    const cl_int   judged_list = judge(properties, ClExternalMemory::handle_properties,
                                       ClExternalMemory::handle_property_count, list);
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
    if (on->external().asked_for(list)) {
        uint64_t     opened = 0u;
        const cl_int taken = on->external().open(*wire, on->guest_device(), list, bytes, opened);
        if (taken != CL_SUCCESS) {
            report_error(error, taken);
            return nullptr;
        }
        /* Not through wrap: what it gives back when the guest cannot keep an
         * object is a clReleaseMemObject, and this cl_mem is the device's own
         * making inside the open. Releasing it behind the device's back would
         * leave the shared object held by a context that can no longer name
         * it, so the object is what goes back. */
        ClObject *object = nullptr;
        if (objects_.make(ClObject::Kind::mem, opened, on, held, object) != Status::ok) {
            static_cast<void>(on->external().give_back(*wire, opened));
            report_error(error, CL_OUT_OF_HOST_MEMORY);
            return nullptr;
        }
        report_error(error, CL_SUCCESS);
        return static_cast<cl_mem>(static_cast<void *>(object));
    }
    /* CL_MEM_USE_HOST_PTR asks for a buffer whose storage is the
     * application's own memory, which the host cannot see. The host's buffer
     * is made as a copy of it instead, and the memory is kept: a map hands it
     * back filled from the buffer and an unmap writes it back, which is what
     * the API promises an application that reads it - only while it is
     * mapped. Affinity makes buffers this way at start. */
    const bool uses_application =
        (flags & static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR)) != 0u;
    if (uses_application &&
        (host_pointer == nullptr ||
         (flags & static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR)) != 0u)) {
        report_error(error, host_pointer == nullptr ? CL_INVALID_HOST_PTR : CL_INVALID_VALUE);
        return nullptr;
    }
    if (uses_application)
        flags = (flags & ~static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR)) |
                static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR);
    const bool            wants_host_memory =
        (flags & static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR)) != 0u &&
        (flags & static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR)) == 0u;
    uint64_t block = 0u;
    bool     backed = false;
    /* wire_of answered, so the context behind it is there. */
    if (wants_host_memory && on->blocks().ready())
        backed = on->blocks().take(bytes, block);

    cl_int   host_error = CL_SUCCESS;
    uint64_t buffer = 0u;
    if (backed) {
        ClBlobBacking backing;
        backing.set(block);
        uint64_t named[4] = {};
        static_cast<void>(backing.write(named));
        buffer = av::clproto::av_clCreateBufferWithProperties(*wire, held->wire(), named, flags,
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
    /* Bytes to copy that one command could not carry go after the create, on
     * a queue of the ICD's own, as an image's do: inside the create they would
     * overflow the staging room and take the context with them. A copy that
     * fits rides inside the create, which costs one round trip rather than
     * four. */
    const bool copies_after = (flags & static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR)) != 0u &&
                              host_pointer != nullptr && bytes > ClTransferChunks::write_room;
    if (buffer == 0u) {
        buffer = av::clproto::av_clCreateBuffer(
            *wire, held->wire(),
            copies_after ? flags & ~static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR) : flags, bytes,
            copies_after ? nullptr : host_pointer, &host_error);
    }
    if (buffer == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    if (copies_after) {
        const cl_int filled =
            fill_buffer(*wire, on->staging(), held->wire(), host_device_of(*held), buffer, bytes, host_pointer);
        if (filled != CL_SUCCESS) {
            give_back_to_host(*wire, ClObject::Kind::mem, buffer);
            report_error(error, filled);
            return nullptr;
        }
    }
    void *const made = wrap(*wire, ClObject::Kind::mem, buffer, held->context(), held, error);
    if (made == nullptr) {
        /* wrap gave the host's buffer back, so only the block is left. */
        if (backed) on->blocks().give_back(block);
        return nullptr;
    }
    if (backed) static_cast<ClObject *>(made)->back_with(block, bytes);
    if (uses_application) static_cast<ClObject *>(made)->use_application_memory(host_pointer);
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
    const uint64_t program = av::clproto::av_clCreateProgramWithSource(*wire, held->wire(), count,
                                                                       sources, lengths, &host_error);
    if (program == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_program>(wrap(*wire, ClObject::Kind::program, program, held->context(), held, error));
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
    const uint64_t kernel = av::clproto::av_clCreateKernel(*wire, held->wire(), name, &host_error);
    if (kernel == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_INVALID_KERNEL_NAME);
        return nullptr;
    }
    return static_cast<cl_kernel>(wrap(*wire, ClObject::Kind::kernel, kernel, held->context(), held, error));
}

cl_int ClIcdRoot::set_kernel_argument(cl_kernel kernel, cl_uint index, size_t bytes,
                                      const void *value)
{
    AV_SCOPE("ClIcdRoot::set_kernel_argument");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* Its only answer is a status, which a kernel the ICD judged live gets
     * wrong only on a misuse: not worth a round trip per argument. */
    icd::UnansweredTransport unanswered(*wire);
    /* An argument of the size of a cl_mem may be one, or a cl_sampler: the
     * application passes the address of its own pointer, and what crosses is
     * the number the host knows that object by. Anything else is bytes. */
    if (bytes == sizeof(cl_mem) && value != nullptr) {
        void *named = nullptr;
        const unsigned char *const from = static_cast<const unsigned char *>(value);
        unsigned char *const        into = reinterpret_cast<unsigned char *>(&named);
        for (size_t index_byte = 0u; index_byte < sizeof named; ++index_byte) {
            into[index_byte] = from[index_byte];
        }
        ClObject *object = judged(named, ClObject::Kind::mem);
        if (object == nullptr) object = judged(named, ClObject::Kind::sampler);
        if (object != nullptr) {
            const uint64_t handle = object->wire();
            return av::clproto::av_clSetKernelArg(unanswered, held->wire(), index, sizeof handle,
                                                   &handle);
        }
    }
    return av::clproto::av_clSetKernelArg(unanswered, held->wire(), index, bytes, value);
}

cl_int ClIcdRoot::enqueue_kernel(cl_command_queue queue, cl_kernel kernel, cl_uint dimensions,
                                 const size_t *offset, const size_t *global, const size_t *local,
                                 cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_kernel");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(kernel, ClObject::Kind::kernel);
    if (named == nullptr) return CL_INVALID_KERNEL;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t     produced = 0u;
    /* An event is the host's to name, so only a launch that wants none goes
     * without waiting for its answer. */
    icd::UnansweredTransport unanswered(*wire);
    const cl_int ran = av::clproto::av_clEnqueueNDRangeKernel(
        event != nullptr ? *wire : unanswered, held->wire(), named->wire(), dimensions, offset, global, local,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (ran == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return ran;
}

cl_int ClIcdRoot::enqueue_external(cl_command_queue queue, cl_uint count, const cl_mem *objects,
                                   cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_external");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (count == 0u || objects == nullptr) return CL_INVALID_VALUE;
    GuestClContext *const context = context_of(*held);
    if (context == nullptr) return CL_INVALID_COMMAND_QUEUE;
    for (cl_uint index = 0u; index < count; index++) {
        ClObject *const named = judged(objects[index], ClObject::Kind::mem);
        if (named == nullptr) return CL_INVALID_MEM_OBJECT;
        /* An ordinary buffer is not an external object, and one of another
         * context is not this queue's to acquire. */
        if (context_of(*named) != context) return CL_INVALID_CONTEXT;
        if (!context->external().answered(named->wire())) return CL_INVALID_MEM_OBJECT;
    }
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    if (event == nullptr && waits.count() == 0u) return CL_SUCCESS;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* What the queue runs after an acquire has to be behind its wait list,
     * which a marker would not hold it to: a barrier does. */
    if (waits.count() != 0u) {
        uint64_t     fenced = 0u;
        const cl_int barred = av::clproto::av_clEnqueueBarrierWithWaitList(
            *wire, held->wire(), waits.count(), listed, event != nullptr ? &fenced : nullptr);
        if (barred != CL_SUCCESS || event == nullptr) return barred;
        *event = static_cast<cl_event>(wrap(*wire, ClObject::Kind::event, fenced, held->context(), held, nullptr));
        return CL_SUCCESS;
    }
    uint64_t     produced = 0u;
    const cl_int marked =
        av::clproto::av_clEnqueueMarkerWithWaitList(*wire, held->wire(), 0u, nullptr, &produced);
    if (marked != CL_SUCCESS) return marked;
    *event = static_cast<cl_event>(
        wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
    return CL_SUCCESS;
}

cl_int ClIcdRoot::enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                               size_t offset, size_t bytes, void *into, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_read");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (into == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
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
     * staging a copy of its own.
     *
     * The marker carries the read's wait list, so the read is reached only
     * once the events it waits for are: a user event the application sets
     * after this returns holds the delivery back, not this call. */
    uint64_t     produced = 0u;
    if (blocking != CL_TRUE) {
        held->busy();
        const cl_int marked = av::clproto::av_clEnqueueMarkerWithWaitList(
            *wire, held->wire(), waits.count(), waits.count() != 0u ? listed : nullptr, &produced);
        if (marked != CL_SUCCESS) return marked;
        /* An event the application did not ask for is one the ICD keeps
         * until the read delivers: it is the row's name, and releasing it
         * now would let the host hand the number to something else. */
        lock_.acquire();
        const bool recorded = reads_.record(produced, held->wire(), named->wire(), offset, bytes, into,
                                            event == nullptr);
        lock_.release();
        if (recorded) {
            if (event != nullptr) {
                *event = static_cast<cl_event>(
                    wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
            }
            return CL_SUCCESS;
        }
        /* No room to remember it. The read still has to deliver, so it is
         * made blocking instead: slower, and never wrong. */
        static_cast<void>(av::clproto::av_clReleaseEvent(*wire, produced));
        log_.write("avcl: too many reads outstanding; this one blocks");
        produced = 0u;
    }
    /* A blocking read is answered inside this one call, so the bytes have to
     * be the host's before it writes the reply: a read the host has only
     * queued has written nothing, and the reply would carry whatever the
     * device's decode arena last held. Asking the host to block on a queue
     * that still has work in it would hold its worker for as long as the
     * queue takes, which is what TdrDelay measures, so the queue is drained
     * first and only then is the read sent blocking, with nothing left for
     * the host to wait on - in pieces the reply slot holds. That is the same
     * order a deferred read is delivered in. */
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     ran = chunks.read(named->wire(), offset, bytes, into);
    if (ran != CL_SUCCESS) return ran;
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                size_t offset, size_t bytes, const void *from, cl_event *event,
                                const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_write");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    /* Complete when this returns, whatever the application asked for, which
     * a non-blocking write may be. The bytes cross inside the command, into
     * the device's decode arena, and the next command decodes over them: a
     * host that read them when its queue reached the write rather than at the
     * call would write the next command into the buffer. So the queue is
     * drained, and the write sent blocking behind nothing, in pieces the
     * staging room carries (ClTransferChunks). */
    static_cast<void>(blocking);
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     ran = chunks.write(named->wire(), offset, bytes, from);
    if (ran != CL_SUCCESS) return ran;
    return marker_event(*wire, *held, event);
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
     * has dxgkrnl reset the adapter. */
    const cl_int waited = drain_unless_idle(*wire, *held);
    if (waited != CL_SUCCESS) return waited;
    /* Everything queued before the marker has run, so every read outstanding
     * on this queue is one the application may now read. */
    return deliver_queued(*wire, context_of(*held)->staging(), held->wire());
}

cl_int ClIcdRoot::flush(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::flush");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clFlush(*wire, held->wire());
}


void *ClIcdRoot::map_buffer(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                            cl_map_flags flags, size_t offset, size_t bytes, cl_event *event,
                            cl_int *error, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::map_buffer");
    ClObject *const held = busied(queue);
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
    /* A map the host answers with a pointer at once, and the staged one reads
     * the bytes in before this returns: neither can carry a wait list the host
     * would block on, so the list is waited for here first. */
    if (waits.count() != 0u || waits.events() != nullptr) {
        const cl_int settled = settle(*wire, *held, waits);
        if (settled != CL_SUCCESS) {
            report_error(error, settled);
            return nullptr;
        }
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
        /* The row names the block this buffer was given, so the arena
         * answers it: the two are written and forgotten together. A
         * sub-buffer's memory is part of its buffer's block, which is the one
         * the arena knows. */
        const ClObject *const owner = named->sliced_from() != nullptr ? named->sliced_from() : named;
        uint8_t *const        block = context->blocks().at(owner->backing_offset(), owner->backing_bytes());
        if (block == nullptr) {
            report_error(error, CL_INVALID_MEM_OBJECT);
            return nullptr;
        }
        uint8_t *const shared = block + (named->backing_offset() - owner->backing_offset());
        {
            uint64_t       settled = 0u;
            cl_int         settle_error = CL_SUCCESS;
            const uint64_t host_mapped = av::clproto::av_clEnqueueMapBuffer(
                *wire, held->wire(), named->wire(), blocking, flags, offset, bytes, 0u, nullptr,
                event != nullptr ? &settled : nullptr, &settle_error);
            if (host_mapped == 0u) {
                refusals_.note_map("clEnqueueMapBuffer: the host refused an arena map", bytes, context->mappings());
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
                refusals_.note_map("clEnqueueMapBuffer: no mapping slot for an arena map", bytes,
                                   context->mappings());
                /* The host's map is undone: one the guest never names again
                 * would be held there for the life of the context. */
                static_cast<void>(av::clproto::av_clEnqueueUnmapMemObject(*wire, held->wire(), named->wire(),
                                                                          host_mapped, 0u, nullptr, nullptr));
                report_error(error, CL_OUT_OF_HOST_MEMORY);
                return nullptr;
            }
            if (event != nullptr) {
                *event = static_cast<cl_event>(
                    wrap(*wire, ClObject::Kind::event, settled, context, held, nullptr));
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
    const uint64_t mapped = av::clproto::av_clEnqueueMapBuffer(
        *wire, held->wire(), named->wire(), blocking, flags, offset, bytes, 0u, nullptr,
        event != nullptr ? &mapped_event : nullptr, &host_error);
    if (mapped == 0u) {
        refusals_.note_map("clEnqueueMapBuffer: the host refused a staged map", bytes, context->mappings());
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_MAP_FAILURE);
        return nullptr;
    }
    /* A buffer over the application's own memory maps to that memory, as the
     * API says it does; any other is staged in a block of the ICD's own.
     * Either is filled here and written back at the unmap. */
    uint8_t *const own = named->application_memory();
    uint8_t *const at = own != nullptr ? context->take_application_mapping(named->wire(), mapped, offset, bytes,
                                                                             flags, own + offset)
                                       : context->take_mapping(named->wire(), mapped, offset, bytes, flags);
    if (at == nullptr) {
        refusals_.note_map("clEnqueueMapBuffer: no slot or no staging block for a map", bytes, context->mappings());
        static_cast<void>(
            av::clproto::av_clEnqueueUnmapMemObject(*wire, held->wire(), named->wire(), mapped, 0u, nullptr, nullptr));
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    /* What the application is entitled to read: the buffer's own bytes. A
     * write-invalidate map promises nothing about them, so it is not read. */
    if ((flags & CL_MAP_WRITE_INVALIDATE_REGION) == 0u) {
        ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
        const cl_int     read = chunks.read(named->wire(), offset, bytes, at);
        if (read != CL_SUCCESS) {
            /* take_mapping answered `at`, so this is the row it took. */
            MappedRegion *const region = context->mapping_at(at);
            context->give_back_mapping(*region);
            static_cast<void>(av::clproto::av_clEnqueueUnmapMemObject(*wire, held->wire(), named->wire(), mapped,
                                                                      0u, nullptr, nullptr));
            report_error(error, read);
            return nullptr;
        }
    }
    if (event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, mapped_event, context, held, nullptr));
    }
    report_error(error, CL_SUCCESS);
    return at;
}

cl_int ClIcdRoot::unmap(cl_command_queue queue, cl_mem buffer, void *pointer, cl_event *event,
                        const ClWaitList &waits)
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
    /* The write-back after the unmap is a transfer the host blocks on. */
    if (waits.count() != 0u || waits.events() != nullptr) {
        const cl_int settled = settle(*wire, *held, waits);
        if (settled != CL_SUCCESS) return settled;
    }
    if (region->image()) return unmap_image(*wire, *held, named->wire(), *region, event);
    held->busy();
    uint64_t     produced = 0u;
    const cl_int unmapped = av::clproto::av_clEnqueueUnmapMemObject(
        *wire, held->wire(), named->wire(), region->host(), 0u, nullptr,
        event != nullptr ? &produced : nullptr);
    /* The write-back follows the unmap, and not the other way round. A host
     * that cannot hand a guest its buffer maps by staging a copy, and its
     * unmap copies that staging area over the buffer - so bytes written
     * before the unmap are exactly the bytes the unmap discards. The mirror
     * is the application's own writes, so it goes last and wins. */
    cl_int written = CL_SUCCESS;
    if (unmapped == CL_SUCCESS && region->writes_back()) {
        ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
        written = chunks.write(named->wire(), region->offset(), region->bytes(), region->at());
    }
    context->give_back_mapping(*region);
    if (unmapped != CL_SUCCESS) return unmapped;
    if (written != CL_SUCCESS) return written;
    if (event != nullptr) {
        *event = static_cast<cl_event>(wrap(*wire, ClObject::Kind::event, produced, context, held, nullptr));
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
    const uint64_t host = av::clproto::av_clSVMAlloc(*wire, held->wire(), flags, bytes, alignment);
    if (host == 0u) return nullptr;
    /* The application is handed the mirror, because it is the pointer the
     * application dereferences; the host address is what crosses the wire. */
    void *const mirror = owner->svm().take(host, bytes);
    if (mirror == nullptr) {
        av::clproto::av_clSVMFree(*wire, held->wire(), host);
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
    if (wire != nullptr) av::clproto::av_clSVMFree(*wire, held->wire(), host);
}

cl_int ClIcdRoot::svm_map(cl_command_queue queue, cl_bool blocking, cl_map_flags flags,
                          void *pointer, size_t bytes, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::svm_map");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    SvmAllocation *const allocation = context->svm().covering(pointer);
    if (allocation == nullptr || bytes == 0u) return CL_INVALID_VALUE;
    /* covering() found the allocation, and an allocation stands for a host
     * address that is never nothing - its own invariant says so. */
    const uint64_t         host = context->svm().host_address_of(pointer);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    /* A blocking map the host would block on behind the list; waited for
     * here instead, and the map sent behind nothing. */
    const bool   waits_here = blocking == CL_TRUE && waits.count() != 0u;
    if (waits_here) {
        const cl_int settled = settle(*wire, *held, waits);
        if (settled != CL_SUCCESS) return settled;
    }
    const cl_uint host_waits = waits_here ? 0u : waits.count();
    uint64_t      produced = 0u;
    const cl_int  mapped = av::clproto::av_clEnqueueSVMMap(
        *wire, held->wire(), blocking, flags, host, bytes, host_waits, host_waits != 0u ? listed : nullptr,
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
        log_.write("avcl: shared memory is mapped, and the mirror is not synchronised yet");
    }
    allocation->mapped();
    if (event != nullptr) {
        *event = static_cast<cl_event>(wrap(*wire, ClObject::Kind::event, produced, context, held, nullptr));
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::svm_unmap(cl_command_queue queue, void *pointer, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::svm_unmap");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    if (context == nullptr) return CL_INVALID_CONTEXT;
    SvmAllocation *const allocation = context->svm().covering(pointer);
    if (allocation == nullptr) return CL_INVALID_VALUE;
    /* covering() found the allocation, and an allocation stands for a host
     * address that is never nothing - its own invariant says so. */
    const uint64_t         host = context->svm().host_address_of(pointer);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    /* Where what the application wrote would go back; see svm_map for why it
     * does not yet. */
    uint64_t     produced = 0u;
    const cl_int unmapped = av::clproto::av_clEnqueueSVMUnmap(
        *wire, held->wire(), host, waits.count(), waits.count() != 0u ? listed : nullptr,
        event != nullptr ? &produced : nullptr);
    if (unmapped == CL_SUCCESS) {
        static_cast<void>(allocation->unmapped());
        if (event != nullptr) {
            *event = static_cast<cl_event>(
                wrap(*wire, ClObject::Kind::event, produced, context, held, nullptr));
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
    return av::clproto::av_clSetKernelArgSVMPointer(*wire, held->wire(), index, host);
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
    const uint64_t event = av::clproto::av_clCreateUserEvent(*wire, held->wire(), &host_error);
    if (event == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_event>(wrap(*wire, ClObject::Kind::event, event, held->context(), held, error));
}

cl_int ClIcdRoot::set_user_event_status(cl_event event, cl_int status)
{
    AV_SCOPE("ClIcdRoot::set_user_event_status");
    ClObject *const held = judged(event, ClObject::Kind::event);
    if (held == nullptr) return CL_INVALID_EVENT;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clSetUserEventStatus(*wire, held->wire(), status);
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
    const cl_int reached = waits_.until_complete(*wire, handles, waited, icd::SlicedWait::forever, context->wakes());
    /* And a wait is where an application learns a non-blocking read is done,
     * so it is where the bytes have to be there. */
    const cl_int delivered = deliver_reads(*wire, context->staging(), handles, waited);
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
        av::clproto::av_clGetEventInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
    /* An application that polls rather than waits learns of the completion
     * here, so this is where the bytes have to be there too. */
    if (asked == CL_SUCCESS && name == CL_EVENT_COMMAND_EXECUTION_STATUS && value != nullptr &&
        value_bytes >= sizeof(cl_int) && *static_cast<const cl_int *>(value) == CL_COMPLETE) {
        const uint64_t handle = held->wire();
        static_cast<void>(deliver_reads(*wire, context_of(*held)->staging(), &handle, 1u));
    }
    return asked;
}

void ClIcdRoot::note_refused(const char *entry_point)
{
    AV_SCOPE("ClIcdRoot::note_refused");
    refusals_.note(entry_point);
}

cl_int ClIcdRoot::noted(const char *entry_point, cl_int result)
{
    AV_SCOPE("ClIcdRoot::noted");
    refusals_.note_failure(entry_point, result);
    return result;
}

cl_int ClIcdRoot::noted_query(const char *entry_point, uint32_t query, cl_int result)
{
    AV_SCOPE("ClIcdRoot::noted_query");
    refusals_.note_query(entry_point, query, result);
    return result;
}

cl_int ClIcdRoot::context_info(cl_context context, cl_context_info name, size_t value_bytes, void *value,
                               size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::context_info");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) return CL_INVALID_CONTEXT;
    /* All of it this ICD's own: its device, the application's count, the list
     * the application gave. The host's context has other answers. */
    const ClInfoRequest asked(value_bytes, value, size_ret);
    switch (name) {
    case CL_CONTEXT_REFERENCE_COUNT: {
        lock_.acquire();
        const cl_uint count = held->references();
        lock_.release();
        return asked.give(&count, sizeof count);
    }
    case CL_CONTEXT_NUM_DEVICES: {
        const cl_uint one = 1u;
        return asked.give(&one, sizeof one);
    }
    case CL_CONTEXT_DEVICES: {
        ClDevice *const    on = devices_.running(*held->context());
        const cl_device_id device = on != nullptr ? reinterpret_cast<cl_device_id>(on->object()) : nullptr;
        return asked.give(&device, sizeof device);
    }
    case CL_CONTEXT_PROPERTIES: {
        cl_context_properties laid[ClPropertyList::max_pairs * 2u + 1u];
        const uint32_t        values = held->context()->properties(laid, ClPropertyList::max_pairs * 2u + 1u);
        return asked.give(laid, values * sizeof(cl_context_properties));
    }
    /* Whether a resource made to be shared would be faster to share: not
     * here, where a texture crosses through a twin whether or not it was made
     * to be shared (docs/d3d11-sharing.md). Asked only of a context made from
     * Direct3D 11. */
    case d3d11_prefer_shared: {
        if (held->context()->d3d11().device() == nullptr) return CL_INVALID_VALUE;
        const cl_bool no = CL_FALSE;
        return asked.give(&no, sizeof no);
    }
    default:
        return CL_INVALID_VALUE;
    }
}

cl_int ClIcdRoot::queue_info(cl_command_queue queue, cl_command_queue_info name, size_t value_bytes, void *value,
                             size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::queue_info");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    const ClInfoRequest asked(value_bytes, value, size_ret);
    switch (name) {
    case CL_QUEUE_CONTEXT: {
        const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
        return asked.give(&owner, sizeof owner);
    }
    case CL_QUEUE_DEVICE: {
        ClDevice *const    on = devices_.running(*held->context());
        const cl_device_id device = on != nullptr ? reinterpret_cast<cl_device_id>(on->object()) : nullptr;
        return asked.give(&device, sizeof device);
    }
    case CL_QUEUE_REFERENCE_COUNT: {
        lock_.acquire();
        const cl_uint count = held->references();
        lock_.release();
        return asked.give(&count, sizeof count);
    }
    case CL_QUEUE_DEVICE_DEFAULT: {
        const cl_command_queue none = nullptr;
        return asked.give(&none, sizeof none);
    }
    default:
        break;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetCommandQueueInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::mem_object_info(cl_mem object, cl_mem_info name, size_t value_bytes, void *value,
                                  size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::mem_object_info");
    ClObject *const held = judged(object, ClObject::Kind::mem);
    if (held == nullptr) return CL_INVALID_MEM_OBJECT;
    /* What names an object is this ICD's handle for it, not the host's; the
     * rest is the host's own answer. */
    const ClInfoRequest asked(value_bytes, value, size_ret);
    switch (name) {
    case CL_MEM_CONTEXT: {
        const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
        return asked.give(&owner, sizeof owner);
    }
    case CL_MEM_ASSOCIATED_MEMOBJECT: {
        const cl_mem over = reinterpret_cast<cl_mem>(held->sliced_from());
        return asked.give(&over, sizeof over);
    }
    case CL_MEM_REFERENCE_COUNT: {
        lock_.acquire();
        const cl_uint count = held->references();
        lock_.release();
        return asked.give(&count, sizeof count);
    }
    /* The host's pointer is nothing in this process: the answer is the
     * application's own memory a buffer was made over, and none for any
     * other (create_image takes none). */
    case CL_MEM_HOST_PTR: {
        const void *const own = held->application_memory();
        return asked.give(&own, sizeof own);
    }
    /* The host made a buffer over the application's memory as a copy of it,
     * and says so; the application asked for its memory to be used. */
    case d3d11_resource_info: {
        const D3d11Object *const made = d3d11_object_of(*held);
        if (made == nullptr) return invalid_d3d11_resource;
        void *const resource = made->resource();
        return asked.give(&resource, sizeof resource);
    }
    case CL_MEM_FLAGS: {
        ClientTransport *const flags_wire = wire_of(*held);
        if (flags_wire == nullptr) return CL_OUT_OF_RESOURCES;
        cl_mem_flags made = 0u;
        const cl_int answered = host_mem_info(*flags_wire, *held, CL_MEM_FLAGS, sizeof made, &made, nullptr);
        if (answered != CL_SUCCESS) return answered;
        if (held->application_memory() != nullptr)
            made = (made & ~static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR)) |
                   static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR);
        return asked.give(&made, sizeof made);
    }
    default:
        break;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return host_mem_info(*wire, *held, name, value_bytes, value, size_ret);
}

/* The map count changes with every map and the property list is not a word;
 * every other answer the host gives here is fixed when the object is made,
 * and a word, so a refusal of one is the host's answer and not the room's. */
cl_int ClIcdRoot::host_mem_info(ClientTransport &wire, ClObject &object, cl_mem_info name, size_t value_bytes,
                                void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::host_mem_info");
    const bool fixed = name == CL_MEM_TYPE || name == CL_MEM_FLAGS || name == CL_MEM_SIZE || name == CL_MEM_OFFSET ||
                       name == CL_MEM_USES_SVM_POINTER;
    if (!fixed) return av::clproto::av_clGetMemObjectInfo(wire, object.wire(), name, value_bytes, value, size_ret);
    const ClInfoRequest asked(value_bytes, value, size_ret);
    cl_int              kept = CL_SUCCESS;
    if (object.kept_answer(name, asked, kept)) return kept;
    uint64_t     answer = 0u;
    size_t       answered = 0u;
    const cl_int fetched =
        av::clproto::av_clGetMemObjectInfo(wire, object.wire(), name, sizeof answer, &answer, &answered);
    if (fetched != CL_SUCCESS) return fetched;
    object.keep_answer(name, &answer, answered);
    return asked.give(&answer, answered);
}

cl_int ClIcdRoot::image_info(cl_mem image, cl_image_info name, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::image_info");
    ClObject *const held = judged(image, ClObject::Kind::mem);
    if (held == nullptr) return CL_INVALID_MEM_OBJECT;
    /* The one answer that is an object: this ICD keeps no buffer under an
     * image, and the host's number for one is nothing here. */
    if (name == CL_IMAGE_BUFFER) {
        const cl_mem none = nullptr;
        return ClInfoRequest(value_bytes, value, size_ret).give(&none, sizeof none);
    }
    if (name == d3d11_subresource_info) {
        const D3d11Object *const made = d3d11_object_of(*held);
        if (made == nullptr) return invalid_d3d11_resource;
        const cl_uint subresource = made->subresource();
        return ClInfoRequest(value_bytes, value, size_ret).give(&subresource, sizeof subresource);
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetImageInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::event_profiling(cl_event event, cl_profiling_info name, size_t value_bytes, void *value,
                                  size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::event_profiling");
    ClObject *const held = judged(event, ClObject::Kind::event);
    if (held == nullptr) return CL_INVALID_EVENT;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetEventProfilingInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
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
    const cl_int told = av::clproto::av_clSetEventCallback(*wire, held->wire(), status);
    if (told != CL_SUCCESS) return told;
    /* The application may release this event on its next line and the
     * callback still has to run and still has to be handed it, so the
     * registration gets a reference of its own. */
    lock_.acquire();
    held->retain();
    lock_.release();
    const cl_int registered = watcher_.watch(held, held->wire(), wire, status, notify, user_data, context->wakes());
    if (registered != CL_SUCCESS) static_cast<void>(release(held, ClObject::Kind::event));
    return registered;
}


cl_int ClIcdRoot::deliver_reads(ClientTransport &wire, ClArenaBlocks &blocks, const uint64_t *events, uint32_t count)
{
    AV_SCOPE("ClIcdRoot::deliver_reads");
    cl_int answer = CL_SUCCESS;
    for (uint32_t index = 0u; index < count; ++index) {
        /* Found, fetched and given back under the lock: the watcher's thread
         * and the application's threads deliver the same reads - a callback
         * on one, a wait on another - and two that both fetched released the
         * marker twice, which the host refused, and the context with it. One
         * that finds the row taken has to find the bytes there too, so it
         * waits for the fetch rather than only the claim. The lock comes
         * before the send lock, as it does for a release. */
        lock_.acquire();
        ClPendingRead *const waiting = reads_.waiting_on(events[index]);
        if (waiting == nullptr) {
            lock_.release();
            continue;
        }
        /* The fetch the enqueue deferred. The host has reached the marker, so
         * everything queued before the read has run and this answers at once;
         * it is sent blocking because there is nothing left to wait for. */
        ClTransferChunks chunks(wire, waiting->queue(), blocks);
        const cl_int     read =
            chunks.read(waiting->buffer(), waiting->offset(), waiting->bytes(), waiting->into());
        if (waiting->ours())
            static_cast<void>(av::clproto::av_clReleaseEvent(wire, waiting->event()));
        waiting->give_up();
        reads_.count_delivery();
        lock_.release();
        if (read != CL_SUCCESS && answer == CL_SUCCESS) answer = read;
    }
    return answer;
}

cl_int ClIcdRoot::deliver_queued(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t queue)
{
    AV_SCOPE("ClIcdRoot::deliver_queued");
    cl_int answer = CL_SUCCESS;
    /* One pass per read rather than one over the table: delivering gives a
     * row up, so the table is walked again each time rather than while it
     * changes underneath. */
    for (uint32_t taken = 0u; taken < ClPendingReads::max_reads; ++taken) {
        uint64_t             event = 0u;
        lock_.acquire();
        for (uint32_t slot = 0u; slot < ClPendingReads::max_reads && event == 0u; ++slot) {
            const ClPendingRead *const row = reads_.at(slot);
            if (row->held() && row->queue() == queue) event = row->event();
        }
        lock_.release();
        if (event == 0u) break;
        const cl_int one = deliver_reads(wire, blocks, &event, 1u);
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
    static_cast<void>(deliver_reads(*wire, context_of(event)->staging(), &handle, 1u));
}

void ClIcdRoot::release_watched(ClObject &event)
{
    AV_SCOPE("ClIcdRoot::release_watched");
    static_cast<void>(release(&event, ClObject::Kind::event));
}

cl_int ClIcdRoot::drain(ClientTransport &wire, uint64_t queue, bool driven, const uint64_t *waits,
                        uint32_t count)
{
    AV_SCOPE("ClIcdRoot::drain");
    uint64_t     marker = 0u;
    const cl_int enqueued =
        av::clproto::av_clEnqueueMarkerWithWaitList(wire, queue, count, count != 0u ? waits : nullptr, &marker);
    if (enqueued != CL_SUCCESS) return enqueued;
    const cl_int waited = waits_.until_queued_completes(wire, queue, marker, driven);
    static_cast<void>(av::clproto::av_clReleaseEvent(wire, marker));
    return waited;
}

ClObject *ClIcdRoot::busied(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::busied");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held != nullptr) held->busy();
    return held;
}

/* What a drain is for - the host never blocks on work still queued, so its
 * worker is never held behind a kernel or a user event - is already true of a
 * queue known idle, and an application reading tiles back one blocking rect
 * at a time otherwise paid a marker, a flush, a poll and a release for each:
 * five round trips where one does. */
cl_int ClIcdRoot::drain_unless_idle(ClientTransport &wire, ClObject &queue)
{
    AV_SCOPE("ClIcdRoot::drain_unless_idle");
    if (queue.known_idle()) return CL_SUCCESS;
    const GuestClContext *const context = queue.context();
    const cl_int drained = drain(wire, queue.wire(), context != nullptr && context->wakes(), nullptr, 0u);
    if (drained == CL_SUCCESS) queue.idle();
    return drained;
}

cl_int ClIcdRoot::marker_event(ClientTransport &wire, ClObject &queue, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::marker_event");
    if (event == nullptr) return CL_SUCCESS;
    uint64_t     produced = 0u;
    const cl_int marked =
        av::clproto::av_clEnqueueMarkerWithWaitList(wire, queue.wire(), 0u, nullptr, &produced);
    if (marked != CL_SUCCESS) return marked;
    /* The transfer is done whether or not an event can be made of the marker,
     * as with every enqueue here: an event the ICD could not keep is a null
     * one, not a failed transfer. */
    *event = static_cast<cl_event>(
        wrap(wire, ClObject::Kind::event, produced, queue.context(), &queue, nullptr));
    return CL_SUCCESS;
}

size_t ClIcdRoot::element_bytes(ClientTransport &wire, uint64_t image)
{
    AV_SCOPE("ClIcdRoot::element_bytes");
    size_t element = 0u;
    if (av::clproto::av_clGetImageInfo(wire, image, CL_IMAGE_ELEMENT_SIZE, sizeof element,
                                        &element, nullptr) != CL_SUCCESS)
        return 0u;
    return element;
}

cl_int ClIcdRoot::program_build_info(cl_program program, cl_device_id device,
                                     cl_program_build_info name, size_t value_bytes, void *value,
                                     size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::program_build_info");
    ClObject *const held = judged(program, ClObject::Kind::program);
    if (held == nullptr) return CL_INVALID_PROGRAM;
    /* The program is its context's, built for that context's one device. */
    ClObject *const named_device = judged(device, ClObject::Kind::device);
    const uint64_t  host_device = host_device_of(*held);
    if (named_device == nullptr || host_device == 0u || named_device->wire() != host_device ||
        devices_.of(*named_device) != devices_.running(*held->context()))
        return CL_INVALID_DEVICE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetProgramBuildInfo(*wire, held->wire(), host_device, name, value_bytes, value,
                                                  size_ret);
}

cl_int ClIcdRoot::kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                         cl_kernel_work_group_info name, size_t value_bytes,
                                         void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::kernel_work_group_info");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    /* A null device is the kernel's only one, which the API allows; any other
     * has to be its context's. */
    const uint64_t host_device = host_device_of(*held);
    if (device != nullptr) {
        ClObject *const named_device = judged(device, ClObject::Kind::device);
        if (named_device == nullptr || devices_.of(*named_device) != devices_.running(*held->context()))
            return CL_INVALID_DEVICE;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetKernelWorkGroupInfo(*wire, held->wire(), host_device, name,
                                                     value_bytes, value, size_ret);
}

cl_int ClIcdRoot::enqueue_copy_buffer(cl_command_queue queue, cl_mem from, cl_mem to,
                                      size_t from_offset, size_t to_offset, size_t bytes,
                                      cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_copy_buffer");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const source = judged(from, ClObject::Kind::mem);
    ClObject *const target = judged(to, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t     produced = 0u;
    /* As a launch: waiting for the answer of a copy that wants no event would
     * wait for every command queued ahead of it, a GPU's worth each time. */
    icd::UnansweredTransport unanswered(*wire);
    const cl_int copied = av::clproto::av_clEnqueueCopyBuffer(
        event != nullptr ? *wire : unanswered, held->wire(), source->wire(), target->wire(), from_offset, to_offset, bytes,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (copied == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return copied;
}

cl_int ClIcdRoot::enqueue_fill_buffer(cl_command_queue queue, cl_mem buffer, const void *pattern,
                                      size_t pattern_bytes, size_t offset, size_t bytes,
                                      cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_fill_buffer");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (pattern == nullptr || pattern_bytes == 0u) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t     produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int filled = av::clproto::av_clEnqueueFillBuffer(
        event != nullptr ? *wire : unanswered, held->wire(), named->wire(), pattern, pattern_bytes, offset, bytes,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (filled == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return filled;
}

cl_int ClIcdRoot::enqueue_copy_image(cl_command_queue queue, cl_mem from, cl_mem to,
                                     const size_t *from_origin, const size_t *to_origin,
                                     const size_t *region, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_copy_image");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const source = judged(from, ClObject::Kind::mem);
    ClObject *const target = judged(to, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from_origin == nullptr || to_origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t     produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int copied = av::clproto::av_clEnqueueCopyImage(
        event != nullptr ? *wire : unanswered, held->wire(), source->wire(), target->wire(), from_origin, to_origin, region,
        waits.count(), waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (copied == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return copied;
}

cl_int ClIcdRoot::fill_image(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t device,
                             uint64_t image,
                             const cl_image_desc &desc, const void *from)
{
    AV_SCOPE("ClIcdRoot::fill_image");
    size_t extent[3] = {desc.image_width, 1u, 1u};
    switch (desc.image_type) {
    case CL_MEM_OBJECT_IMAGE1D: break;
    case CL_MEM_OBJECT_IMAGE2D: extent[1] = desc.image_height; break;
    case CL_MEM_OBJECT_IMAGE2D_ARRAY:
        extent[1] = desc.image_height;
        extent[2] = desc.image_array_size;
        break;
    case CL_MEM_OBJECT_IMAGE3D:
        extent[1] = desc.image_height;
        extent[2] = desc.image_depth;
        break;
    /* A 1D array is pitched by its slice pitch alone, and a buffer image is
     * its buffer's bytes: neither is rows and slices. */
    default: return CL_INVALID_OPERATION;
    }
    const size_t        origin[3] = {0u, 0u, 0u};
    const ClImageRegion region(origin, extent, desc.image_row_pitch, desc.image_slice_pitch,
                               element_bytes(wire, image));
    if (!region.valid()) return CL_INVALID_IMAGE_DESCRIPTOR;
    cl_int         made = CL_SUCCESS;
    const uint64_t queue = own_queue(wire, context, device, made);
    if (queue == 0u) return made;
    ClTransferChunks chunks(wire, queue, blocks);
    const cl_int     written = chunks.write_image(image, region, from);
    static_cast<void>(av::clproto::av_clReleaseCommandQueue(wire, queue));
    return written;
}

cl_int ClIcdRoot::fill_buffer(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t context, uint64_t device,
                              uint64_t buffer,
                              size_t bytes,
                              const void *from)
{
    AV_SCOPE("ClIcdRoot::fill_buffer");
    cl_int         made = CL_SUCCESS;
    const uint64_t queue = own_queue(wire, context, device, made);
    if (queue == 0u) return made;
    ClTransferChunks chunks(wire, queue, blocks);
    const cl_int     written = chunks.write(buffer, 0u, bytes, from);
    static_cast<void>(av::clproto::av_clReleaseCommandQueue(wire, queue));
    return written;
}

uint64_t ClIcdRoot::own_queue(ClientTransport &wire, uint64_t context, uint64_t device, cl_int &error)
{
    AV_SCOPE("ClIcdRoot::own_queue");
    cl_int         made = CL_SUCCESS;
    const uint64_t queue =
        av::clproto::av_clCreateCommandQueueWithProperties(wire, context, device, nullptr, &made);
    if (queue == 0u) error = made != CL_SUCCESS ? made : CL_OUT_OF_RESOURCES;
    return queue;
}

cl_mem ClIcdRoot::create_image_2d(cl_context context, cl_mem_flags flags, const cl_image_format *format,
                                  size_t width, size_t height, size_t row_pitch, void *host_pointer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_image_2d");
    cl_image_desc desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE2D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_row_pitch = row_pitch;
    return create_image(context, flags, format, &desc, host_pointer, error);
}

cl_mem ClIcdRoot::create_image_3d(cl_context context, cl_mem_flags flags, const cl_image_format *format,
                                  size_t width, size_t height, size_t depth, size_t row_pitch, size_t slice_pitch,
                                  void *host_pointer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_image_3d");
    cl_image_desc desc = {};
    desc.image_type = CL_MEM_OBJECT_IMAGE3D;
    desc.image_width = width;
    desc.image_height = height;
    desc.image_depth = depth;
    desc.image_row_pitch = row_pitch;
    desc.image_slice_pitch = slice_pitch;
    return create_image(context, flags, format, &desc, host_pointer, error);
}

cl_mem ClIcdRoot::create_image(cl_context context, cl_mem_flags flags,
                               const cl_image_format *format, const cl_image_desc *desc,
                               void *host_pointer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_image");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (format == nullptr) {
        report_error(error, CL_INVALID_IMAGE_FORMAT_DESCRIPTOR);
        return nullptr;
    }
    if (desc == nullptr) {
        report_error(error, CL_INVALID_IMAGE_DESCRIPTOR);
        return nullptr;
    }
    if ((flags & CL_MEM_USE_HOST_PTR) != 0u) {
        log_.write("avcl: an image over the application's own memory is not carried");
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    /* The API's own pairing: a pointer exactly when the flags say to copy. */
    const bool copies = (flags & CL_MEM_COPY_HOST_PTR) != 0u;
    if (copies != (host_pointer != nullptr)) {
        report_error(error, CL_INVALID_HOST_PTR);
        return nullptr;
    }
    cl_image_desc described = *desc;
    if (desc->buffer != nullptr) {
        ClObject *const buffer = judged(desc->buffer, ClObject::Kind::mem);
        if (buffer == nullptr) {
            report_error(error, CL_INVALID_IMAGE_DESCRIPTOR);
            return nullptr;
        }
        /* What crosses is the host's number for it, which the decoder turns
         * back into the host's cl_mem. */
        described.buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(buffer->wire()));
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t image = av::clproto::av_clCreateImage(
        *wire, held->wire(), flags & ~static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR), format,
        &described, nullptr, 0u, &host_error);
    if (image == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    if (copies) {
        const cl_int filled = fill_image(*wire, context_of(*held)->staging(), held->wire(), host_device_of(*held),
                                         image, described, host_pointer);
        if (filled != CL_SUCCESS) {
            give_back_to_host(*wire, ClObject::Kind::mem, image);
            report_error(error, filled);
            return nullptr;
        }
    }
    return static_cast<cl_mem>(wrap(*wire, ClObject::Kind::mem, image, held->context(), held, error));
}

cl_int ClIcdRoot::enqueue_read_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                                     const size_t *region, size_t row_pitch, size_t slice_pitch,
                                     void *into, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_read_image");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(image, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (origin == nullptr || region == nullptr || into == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const ClImageRegion described(origin, region, row_pitch, slice_pitch,
                                  element_bytes(*wire, named->wire()));
    if (!described.valid()) return CL_INVALID_VALUE;
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     read = chunks.read_image(named->wire(), described, into);
    if (read != CL_SUCCESS) return read;
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::enqueue_write_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                                      const size_t *region, size_t row_pitch, size_t slice_pitch,
                                      const void *from, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_write_image");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(image, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (origin == nullptr || region == nullptr || from == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const ClImageRegion described(origin, region, row_pitch, slice_pitch,
                                  element_bytes(*wire, named->wire()));
    if (!described.valid()) return CL_INVALID_VALUE;
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     written = chunks.write_image(named->wire(), described, from);
    if (written != CL_SUCCESS) return written;
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::enqueue_read_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                           void *into, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_read_buffer_rect");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (into == nullptr) return CL_INVALID_VALUE;
    const cl_int checked = rect.check();
    if (checked != CL_SUCCESS) return checked;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     read = chunks.read_buffer_rect(named->wire(), rect, into);
    if (read != CL_SUCCESS) return read;
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::enqueue_write_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                            const void *from, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_write_buffer_rect");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from == nullptr) return CL_INVALID_VALUE;
    const cl_int checked = rect.check();
    if (checked != CL_SUCCESS) return checked;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     written = chunks.write_buffer_rect(named->wire(), rect, from);
    if (written != CL_SUCCESS) return written;
    return marker_event(*wire, *held, event);
}

cl_int ClIcdRoot::enqueue_copy_buffer_rect(cl_command_queue queue, cl_mem from, cl_mem to,
                                           const size_t *from_origin, const size_t *to_origin, const size_t *region,
                                           size_t from_row_pitch, size_t from_slice_pitch, size_t to_row_pitch,
                                           size_t to_slice_pitch, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_copy_buffer_rect");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClObject *const source = judged(from, ClObject::Kind::mem);
    ClObject *const target = judged(to, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from_origin == nullptr || to_origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     listed[max_waited] = {};
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t                 produced = 0u;
    icd::UnansweredTransport unanswered(*wire);
    const cl_int             copied = av::clproto::av_clEnqueueCopyBufferRect(
        event != nullptr ? *wire : unanswered, held->wire(), source->wire(), target->wire(), from_origin, to_origin,
        region, from_row_pitch, from_slice_pitch, to_row_pitch, to_slice_pitch, waits.count(),
        waits.count() != 0u ? listed : nullptr, event != nullptr ? &produced : nullptr);
    if (copied == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr));
    }
    return copied;
}

cl_sampler ClIcdRoot::create_sampler(cl_context context, const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_sampler");
    static constexpr uint64_t allowed[] = {static_cast<uint64_t>(CL_SAMPLER_NORMALIZED_COORDS),
                                           static_cast<uint64_t>(CL_SAMPLER_ADDRESSING_MODE),
                                           static_cast<uint64_t>(CL_SAMPLER_FILTER_MODE)};
    ClPropertyList            list;
    const cl_int              judged_list = judge(properties, allowed, 3u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list);
        return nullptr;
    }
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
    /* The values are the host's to judge: which modes pair with which is the
     * device's own rule. */
    cl_int         host_error = CL_SUCCESS;
    const uint64_t sampler =
        av::clproto::av_clCreateSamplerWithProperties(*wire, held->wire(), properties, &host_error);
    if (sampler == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return static_cast<cl_sampler>(
        wrap(*wire, ClObject::Kind::sampler, sampler, held->context(), held, error));
}

}  // namespace av::clicd

namespace av::clicd {

cl_int ClIcdRoot::supported_image_formats(cl_context context, cl_mem_flags flags,
                                          cl_mem_object_type type, cl_uint num_entries,
                                          cl_image_format *formats, cl_uint *num_formats)
{
    AV_SCOPE("ClIcdRoot::supported_image_formats");
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) return CL_INVALID_CONTEXT;
    if (formats != nullptr && num_entries == 0u) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const size_t room = formats != nullptr ? num_entries * sizeof(cl_image_format) : 0u;
    return av::clproto::av_clGetSupportedImageFormats(*wire, held->wire(), flags, type, num_entries,
                                                       formats, room, num_formats);
}

cl_int ClIcdRoot::wait_list(GuestClContext &context, cl_uint count, const cl_event *events,
                            uint64_t *into)
{
    AV_SCOPE("ClIcdRoot::wait_list");
    if ((count == 0u) != (events == nullptr)) return CL_INVALID_EVENT_WAIT_LIST;
    if (count > max_waited) return CL_OUT_OF_RESOURCES;
    for (cl_uint index = 0u; index < count; ++index) {
        ClObject *const held = judged(events[index], ClObject::Kind::event);
        if (held == nullptr || held->context() != &context) return CL_INVALID_EVENT_WAIT_LIST;
        into[index] = held->wire();
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::enqueue_point(PointCommand command, cl_command_queue queue, cl_uint count,
                                const cl_event *events, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_point");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    uint64_t     handles[max_waited] = {};
    const cl_int listed = wait_list(*context, count, events, handles);
    if (listed != CL_SUCCESS) return listed;
    uint64_t     produced = 0u;
    const cl_int placed = command(*wire, held->wire(), count, count != 0u ? handles : nullptr,
                                  event != nullptr ? &produced : nullptr);
    if (placed == CL_SUCCESS && event != nullptr) {
        *event = static_cast<cl_event>(
            wrap(*wire, ClObject::Kind::event, produced, context, held, nullptr));
    }
    return placed;
}

cl_int ClIcdRoot::gl_share_for(const ClPropertyList &list, uint32_t &share)
{
    AV_SCOPE("ClIcdRoot::gl_share_for");
    share = 0u;
    uint64_t gl_context = 0u;
    if (!list.names(static_cast<uint64_t>(CL_GL_CONTEXT_KHR), gl_context) || gl_context == 0u) return CL_SUCCESS;
    share = gl_share_.share_name(reinterpret_cast<void *>(static_cast<uintptr_t>(gl_context)));
    return share != 0u ? CL_SUCCESS : CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
}

ClObject *ClIcdRoot::gl_sharing(cl_context context, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::gl_sharing");
    ClObject *const       held = judged(context, ClObject::Kind::context);
    GuestClContext *const guest = held != nullptr ? held->context() : nullptr;
    /* A context made without a GL context still has a GL context of the
     * host's own behind it (feature 053's vehicle), in which the application's
     * GL names mean nothing - or something else. */
    if (guest == nullptr || !guest->shares_gl()) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
    if (wire_of(*held) == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    return held;
}

cl_mem ClIcdRoot::gl_object(ClientTransport &wire, ClObject &context, uint64_t made, cl_int host_error,
                            cl_int *error)
{
    AV_SCOPE("ClIcdRoot::gl_object");
    if (made == 0u) {
        report_error(error, host_error != CL_SUCCESS ? host_error : CL_INVALID_GL_OBJECT);
        return nullptr;
    }
    return static_cast<cl_mem>(wrap(wire, ClObject::Kind::mem, made, context.context(), &context, error));
}

cl_int ClIcdRoot::device_ids_from_d3d10(cl_platform_id platform, cl_uint source, void *object, cl_uint set,
                                        cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("ClIcdRoot::device_ids_from_d3d10");
    if (!platform_.names_this(platform)) return CL_INVALID_PLATFORM;
    if ((source != d3d10_device_source && source != d3d10_adapter_source) ||
        (set != d3d10_preferred_devices && set != d3d10_all_devices) ||
        (devices == nullptr && num_devices == nullptr) || (devices != nullptr && num_entries == 0u))
        return CL_INVALID_VALUE;
    const uint64_t luid =
        source == d3d10_device_source ? d3d_adapters_.device_luid(object) : d3d_adapters_.adapter_luid(object);
    return device_ids_for_luid(luid, devices, num_devices);
}

cl_int ClIcdRoot::device_ids_for_luid(uint64_t luid, cl_device_id *devices, cl_uint *num_devices)
{
    AV_SCOPE("ClIcdRoot::device_ids_for_luid");
    /* Only the asked adapter's device is opened: an application asking about
     * each of its adapters in turn opens no device it was not given. */
    lock_.acquire();
    learn_devices();
    ClDevice *const device = devices_.on(luid);
    const bool      opened = device != nullptr && open_device(*device) == Status::ok;
    lock_.release();
    if (!opened) {
        if (num_devices != nullptr) *num_devices = 0u;
        return CL_DEVICE_NOT_FOUND;
    }
    if (devices != nullptr) devices[0] = reinterpret_cast<cl_device_id>(device->object());
    if (num_devices != nullptr) *num_devices = 1u;
    return CL_SUCCESS;
}

cl_mem ClIcdRoot::create_from_d3d10(cl_context context, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_d3d10");
    report_error(error, CL_INVALID_CONTEXT);
    static_cast<void>(context);
    return nullptr;
}

cl_int ClIcdRoot::enqueue_d3d10_objects(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::enqueue_d3d10_objects");
    return busied(queue) != nullptr ? CL_INVALID_CONTEXT : CL_INVALID_COMMAND_QUEUE;
}

cl_mem ClIcdRoot::create_from_gl_buffer(cl_context context, cl_mem_flags flags, cl_GLuint buffer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_gl_buffer");
    ClObject *const held = gl_sharing(context, error);
    if (held == nullptr) return nullptr;
    ClientTransport &wire = *wire_of(*held);
    cl_int           host_error = CL_SUCCESS;
    const uint64_t   made = av::clproto::av_clCreateFromGLBuffer(wire, held->wire(), flags, buffer, &host_error);
    return gl_object(wire, *held, made, host_error, error);
}

cl_mem ClIcdRoot::create_from_gl_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                         cl_GLuint texture, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_gl_texture");
    ClObject *const held = gl_sharing(context, error);
    if (held == nullptr) return nullptr;
    ClientTransport &wire = *wire_of(*held);
    cl_int           host_error = CL_SUCCESS;
    const uint64_t   made =
        av::clproto::av_clCreateFromGLTexture(wire, held->wire(), flags, target, level, texture, &host_error);
    return gl_object(wire, *held, made, host_error, error);
}

cl_mem ClIcdRoot::create_from_gl_renderbuffer(cl_context context, cl_mem_flags flags, cl_GLuint renderbuffer,
                                              cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_gl_renderbuffer");
    ClObject *const held = gl_sharing(context, error);
    if (held == nullptr) return nullptr;
    ClientTransport &wire = *wire_of(*held);
    cl_int           host_error = CL_SUCCESS;
    const uint64_t   made =
        av::clproto::av_clCreateFromGLRenderbuffer(wire, held->wire(), flags, renderbuffer, &host_error);
    return gl_object(wire, *held, made, host_error, error);
}

cl_int ClIcdRoot::gl_object_info(cl_mem object, cl_gl_object_type *type, cl_GLuint *name)
{
    AV_SCOPE("ClIcdRoot::gl_object_info");
    ClObject *const named = judged(object, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    ClientTransport *const wire = wire_of(*named);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetGLObjectInfo(*wire, named->wire(), type, name);
}

cl_int ClIcdRoot::gl_texture_info(cl_mem object, cl_gl_texture_info name, size_t value_bytes, void *value,
                                  size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::gl_texture_info");
    ClObject *const named = judged(object, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    ClientTransport *const wire = wire_of(*named);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetGLTextureInfo(*wire, named->wire(), name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::enqueue_acquire_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                     const cl_event *events, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_acquire_gl");
    return enqueue_gl_objects(&av::clproto::av_clEnqueueAcquireGLObjects, queue, count, objects, waits, events,
                              event);
}

cl_int ClIcdRoot::enqueue_release_gl(cl_command_queue queue, cl_uint count, const cl_mem *objects, cl_uint waits,
                                     const cl_event *events, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_release_gl");
    return enqueue_gl_objects(&av::clproto::av_clEnqueueReleaseGLObjects, queue, count, objects, waits, events,
                              event);
}

cl_int ClIcdRoot::enqueue_gl_objects(GlObjectsCommand command, cl_command_queue queue, cl_uint count,
                                     const cl_mem *objects, cl_uint waits, const cl_event *events, cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_gl_objects");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    GuestClContext *const context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (count != 0u && objects == nullptr) return CL_INVALID_VALUE;
    if (count > max_waited) return CL_OUT_OF_RESOURCES;
    uint64_t named[max_waited] = {};
    for (cl_uint index = 0u; index < count; index++) {
        ClObject *const object = judged(objects[index], ClObject::Kind::mem);
        if (object == nullptr) return CL_INVALID_MEM_OBJECT;
        if (context_of(*object) != context) return CL_INVALID_CONTEXT;
        named[index] = object->wire();
    }
    uint64_t     handles[max_waited] = {};
    const cl_int listed = wait_list(*context, waits, events, handles);
    if (listed != CL_SUCCESS) return listed;
    uint64_t     produced = 0u;
    const cl_int placed = command(*wire, held->wire(), count, count != 0u ? named : nullptr, waits,
                                  waits != 0u ? handles : nullptr, event != nullptr ? &produced : nullptr);
    if (placed == CL_SUCCESS && event != nullptr)
        *event = static_cast<cl_event>(wrap(*wire, ClObject::Kind::event, produced, context, held, nullptr));
    return placed;
}

cl_int ClIcdRoot::gl_context_info(const cl_context_properties *properties, cl_gl_context_info name,
                                  size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::gl_context_info");
    static constexpr uint64_t allowed[] = {
        static_cast<uint64_t>(CL_CONTEXT_PLATFORM), static_cast<uint64_t>(CL_CONTEXT_INTEROP_USER_SYNC),
        static_cast<uint64_t>(CL_GL_CONTEXT_KHR), static_cast<uint64_t>(CL_WGL_HDC_KHR)};
    ClPropertyList list;
    const cl_int   judged_list =
        judge(properties, allowed, 4u, list);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t platform = 0u;
    if (list.names(static_cast<uint64_t>(CL_CONTEXT_PLATFORM), platform) && platform != 0u &&
        !platform_.names_this(reinterpret_cast<cl_platform_id>(platform)))
        return CL_INVALID_PLATFORM;
    uint32_t     share = 0u;
    const cl_int shared = gl_share_for(list, share);
    if (shared != CL_SUCCESS) return shared;
    if (share == 0u) return CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR;
    if (name != CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR && name != CL_DEVICES_FOR_GL_CONTEXT_KHR)
        return CL_INVALID_VALUE;
    /* The OpenGL ICD runs every context on the adapter the primary display is
     * on, which AvgpuAdapters lists first, as the first device is. */
    lock_.acquire();
    learn_devices();
    ClDevice *const device = devices_.count() != 0u ? &devices_.at(0u) : nullptr;
    const bool      opened = device != nullptr && open_device(*device) == Status::ok;
    lock_.release();
    if (!opened) return CL_OUT_OF_RESOURCES;
    /* A device that does not report cl_khr_gl_sharing - the executor strikes it
     * where the host's OpenCL refused its GL context, as NVIDIA's refuses
     * every EGL one - can share with no GL context: answered as no device,
     * rather than one whose clCreateContext from this GL context must fail. */
    char         reported[host_extensions_bytes] = {};
    const cl_int listed = device_extensions(*device, sizeof reported, reported, nullptr);
    if (listed != CL_SUCCESS) return listed;
    const bool shares = device->extensions().offered_by(reported, sizeof reported, "cl_khr_gl_sharing");
    if (size_ret != nullptr) *size_ret = shares ? sizeof(cl_device_id) : 0u;
    if (value == nullptr || !shares) return CL_SUCCESS;
    if (value_bytes < sizeof(cl_device_id)) return CL_INVALID_VALUE;
    *static_cast<cl_device_id *>(value) = reinterpret_cast<cl_device_id>(device->object());
    return CL_SUCCESS;
}

cl_int ClIcdRoot::enqueue_marker(cl_command_queue queue, cl_uint count, const cl_event *events,
                                 cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_marker");
    return enqueue_point(&av::clproto::av_clEnqueueMarkerWithWaitList, queue, count, events, event);
}

cl_int ClIcdRoot::enqueue_barrier(cl_command_queue queue, cl_uint count, const cl_event *events,
                                  cl_event *event)
{
    AV_SCOPE("ClIcdRoot::enqueue_barrier");
    return enqueue_point(&av::clproto::av_clEnqueueBarrierWithWaitList, queue, count, events, event);
}

}  // namespace av::clicd
