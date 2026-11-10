#include "icd/opencl/cl_icd_root.hpp"

#include "core/support/extension_words.hpp"

#include "icd/opencl/cl_extension_policy.hpp"

#include "icd/opencl/d3d11_bridge.hpp"
#include "icd/opencl/d3d11_object.hpp"

#include "guest/kmt/avgpu_adapters.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/kmt/pci_identity.hpp"
#include "icd/common/adapter_facts.hpp"
#include "icd/common/adapter_memory.hpp"

#include "icd/opencl/cl_build_options.hpp"
#include "icd/opencl/cl_info_request.hpp"
#include "icd/opencl/cl_mem_flags.hpp"

#include "guest/transport/host_allocation.hpp"
#include "proto/resources/cl_blob_backing.hpp"

#include "core/trace/report_gate.hpp"
#include "icd/common/sliced_wait.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/client/transfer_room.hpp"
#include "core/trace/state_writer.hpp"
#include "guest/kmt/kmt.hpp"
#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "icd/opencl/unanswered_transport.hpp"
#include "icd/opencl/cl_external_memory.hpp"
#include "icd/opencl/cl_icd_binding.hpp"
#include "icd/opencl/cl_image_region.hpp"
#include "icd/opencl/cl_buffer_rect.hpp"
#include "icd/opencl/cl_transfer_chunks.hpp"
#include "icd/opencl/d3d_adapter_source.hpp"
#include "icd/opencl/gl_share_source.hpp"
#include "icd/opencl/header_source.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {
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

constexpr uint64_t kib = 1024u;

}

ClIcdRoot::ClIcdRoot(BlockSource &memory, Kmt &kmt, ThreadLock &lock,
                     ThreadLock &send_lock, ThreadLock &objects_lock, ThreadLock &blocks_lock,
                     KeyedLog &log,
                     icd::ServiceThread &watcher_thread, GlShareSource &gl_share, D3dAdapterSource &d3d_adapters,
                     D3d11Bridge &d3d11, HeaderSource &headers,
                     ClIcdBinding &binding)
    : lock_(lock), lines_(log), gl_share_(gl_share), d3d_adapters_(d3d_adapters), d3d11_(d3d11), headers_(headers), binding_(binding), dispatch_(), platform_(),
      objects_(memory, dispatch_.table(), objects_lock), kmt_(kmt), devices_(memory, kmt),
      shared_handles_(kmt, lock, memory), contexts_(memory), shapes_(memory), sources_(memory), memory_(memory), send_lock_(send_lock), blocks_lock_(blocks_lock),
      watcher_(memory, watcher_thread, log, lock, *this), waits_(kmt, watcher_), device_answers_(memory), reads_(), refused_(lines_), refusals_(lines_),
      destructor_callbacks_(memory)
{
    AV_SCOPE("ClIcdRoot::ClIcdRoot");
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
        const ClExtensionPolicy platform_wide(reported_extensions, reported_extension_count);
        const ExtensionWords    no_host(memory_, Pool::paged);
        const size_t            needed = platform_wide.report(no_host, nullptr, 0u);
        if (value != nullptr) {
            if (value_bytes < needed) return CL_INVALID_VALUE;
            static_cast<void>(platform_wide.report(no_host, static_cast<char *>(value), value_bytes));
        }
        if (size_ret != nullptr) *size_ret = needed;
        return CL_SUCCESS;
    }
    if (name == CL_PLATFORM_EXTENSIONS_WITH_VERSION) {
        if (!platform_.names_this(platform)) return CL_INVALID_PLATFORM;
        return platform_extension_versions(value_bytes, value, size_ret);
    }
    if (name == CL_PLATFORM_NAME) {
        const ThreadLockHold locked(lock_);
        learn_devices();
    }
    return platform_.info(platform, name, value_bytes, value, size_ret);
}

void ClIcdRoot::learn_devices()
{
    AV_SCOPE("ClIcdRoot::learn_devices");
    if (devices_.learnt() && (devices_.count() != 1u || platform_.names_an_adapter())) return;
    AvgpuAdapters adapters(kmt_);
    if (adapters.learn() != Status::ok) return;
    if (!devices_.learnt()) {
        devices_.learn(adapters);
        if (devices_.left_out() != 0u)
            lines_.write("an avgpu adapter is left out: Windows gave it no LUID");
    }
    for (uint32_t index = 0u; index < devices_.count(); ++index) {
        ClDevice &device = devices_.at(index);
        describe_adapter(adapters.adapter(adapters.index_of(device.luid())), device);
    }
    if (devices_.count() == 1u) platform_.observe_adapter(devices_.at(0u).decided().adapter_name());
}

void ClIcdRoot::describe_adapter(uint32_t adapter, ClDevice &device)
{
    AV_SCOPE("ClIcdRoot::describe_adapter");
    constexpr uint32_t described =
        icd::AdapterFacts::identity_fact | icd::AdapterFacts::location_fact | icd::AdapterFacts::name_fact;
    icd::AdapterFacts facts;
    facts.learn(kmt_, adapter);
    if (!facts.knows(described)) return;
    device.decided().observe_adapter(facts);
    device.extensions().observe_vendor(facts.identity().vendor());
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
    if (spells(name, "clIcdGetPlatformIDsKHR")) return dispatch_.icd_get_platform_ids();
    if (spells(name, "clEnqueueAcquireExternalMemObjectsKHR"))
        return dispatch_.external_mem_acquire();
    if (spells(name, "clEnqueueReleaseExternalMemObjectsKHR"))
        return dispatch_.external_mem_release();
    if (spells(name, "clGetGLContextInfoKHR")) return dispatch_.gl_context_info();
    if (spells(name, "clCreateProgramWithILKHR")) return dispatch_.program_from_il();
    return nullptr;
}

Status ClIcdRoot::open_device(ClDevice &device, bool &exhausted)
{
    AV_SCOPE("ClIcdRoot::open_device");
    exhausted = false;
    if (device.object() != nullptr) return Status::ok;
    GuestClContext *context = nullptr;
    const Status    made =
        contexts_.create(context, device.connection(), memory_, send_lock_, blocks_lock_, lines_, shared_handles_);
    if (made != Status::ok) {
        lines_.write("no room for the ICD's own context");
        return made;
    }
    const Status           opened = context->make();
    ClientTransport *const wire = opened == Status::ok ? context->wire() : nullptr;
    if (wire == nullptr) {
        lines_.write("the ICD's own context could not be made");
        exhausted = context->exhausted();
        static_cast<void>(contexts_.destroy(context));
        return opened;
    }
    uint64_t host_platform = 0u;
    cl_uint  platforms = 0u;
    if (av::clproto::av_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) != CL_SUCCESS ||
        platforms == 0u || host_platform == 0u) {
        exhausted = context->exhausted();
        static_cast<void>(contexts_.destroy(context));
        return Status::not_supported;
    }
    uint64_t host_device = 0u;
    cl_uint  devices = 0u;
    if (av::clproto::av_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device,
                                        &devices) != CL_SUCCESS ||
        devices == 0u || host_device == 0u) {
        exhausted = context->exhausted();
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
    constexpr cl_device_type known_types = CL_DEVICE_TYPE_DEFAULT | CL_DEVICE_TYPE_CPU | CL_DEVICE_TYPE_GPU |
                                           CL_DEVICE_TYPE_ACCELERATOR | CL_DEVICE_TYPE_CUSTOM;
    if (type == 0u || (type != CL_DEVICE_TYPE_ALL && (type & ~known_types) != 0u)) return CL_INVALID_DEVICE_TYPE;
    const bool wants_gpu = (type & (CL_DEVICE_TYPE_GPU | CL_DEVICE_TYPE_DEFAULT)) != 0u;
    if (!wants_gpu) {
        if (num_devices != nullptr) *num_devices = 0u;
        return CL_DEVICE_NOT_FOUND;
    }
    cl_uint found = 0u;
    bool    exhausted = false;
    {
        const ThreadLockHold locked(lock_);
        learn_devices();
        for (uint32_t index = 0u; index < devices_.count(); ++index) {
            ClDevice &device = devices_.at(index);
            bool         refused = false;
            const Status opened = open_device(device, refused);
            exhausted = exhausted || refused;
            if (opened != Status::ok) continue;
            if (devices != nullptr && found < num_entries)
                devices[found] = reinterpret_cast<cl_device_id>(device.object());
            ++found;
        }
    }
    if (num_devices != nullptr) *num_devices = found;
    if (found != 0u) return CL_SUCCESS;
    return exhausted ? CL_OUT_OF_RESOURCES : CL_DEVICE_NOT_FOUND;
}

uint64_t ClIcdRoot::host_device_of(ClObject &object)
{
    AV_SCOPE("ClIcdRoot::host_device_of");
    GuestClContext *const context = context_of(object);
    ClDevice *const       device = context != nullptr ? devices_.running(*context) : nullptr;
    return device != nullptr ? device->object()->wire() : 0u;
}

cl_int ClIcdRoot::device_info(cl_device_id device, cl_device_info name, size_t value_bytes,
                              void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::device_info");
    ClObject *const object = judged(device, ClObject::Kind::device);
    ClDevice *const shown = object != nullptr ? devices_.of(*object) : nullptr;
    if (shown == nullptr) return CL_INVALID_DEVICE;
    if (shown->decided().decides(name)) return shown->decided().info(name, value_bytes, value, size_ret);
    if (name == CL_DEVICE_PLATFORM) {
        const cl_platform_id ours = platform_.as_handle();
        return ClInfoRequest(value_bytes, value, size_ret).give(&ours, sizeof ours);
    }
    if (name == CL_DEVICE_PARENT_DEVICE) {
        const cl_device_id none = nullptr;
        return ClInfoRequest(value_bytes, value, size_ret).give(&none, sizeof none);
    }
    if (name == CL_DEVICE_EXTENSIONS) return device_extensions(*shown, value_bytes, value, size_ret);
    if (name == CL_DEVICE_EXTENSIONS_WITH_VERSION)
        return device_extension_versions(*shown, value_bytes, value, size_ret);
    if (name == CL_DEVICE_OPENCL_C_FEATURES) return c_features(*shown, value_bytes, value, size_ret);
    if (name == CL_DEVICE_EXTERNAL_MEMORY_IMPORT_HANDLE_TYPES_KHR)
        return import_handle_types(*shown, value_bytes, value, size_ret);
    if (name == free_memory_amd && shown->decided().on_vendor(PciIdentity::amd_vendor))
        return free_memory(*shown, value_bytes, value, size_ret);
    ClientTransport *const wire = wire_of(*object);
    if (wire == nullptr) return CL_INVALID_DEVICE;
    return host_device_info(*wire, *shown, name, value_bytes, value, size_ret);
}

cl_int ClIcdRoot::free_memory(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::free_memory");
    icd::AdapterMemory memory(kmt_, device.connection().adapter());
    if (memory.read() != Status::ok) return CL_OUT_OF_RESOURCES;
    size_t free_kib = static_cast<size_t>(memory.available(MemorySegmentGroup::local) / kib);
    ClientTransport *const wire = wire_of(*device.object());
    size_t                 host_kib[2] = {};
    cl_ulong               total_bytes = 0u;
    if (wire != nullptr &&
        av::clproto::av_clGetDeviceInfo(*wire, device.object()->wire(), free_memory_amd, sizeof host_kib, host_kib,
                                         nullptr) == CL_SUCCESS &&
        av::clproto::av_clGetDeviceInfo(*wire, device.object()->wire(), CL_DEVICE_GLOBAL_MEM_SIZE, sizeof total_bytes,
                                         &total_bytes, nullptr) == CL_SUCCESS) {
        const size_t total_kib = static_cast<size_t>(total_bytes / kib);
        const size_t held_kib = total_kib > host_kib[0] ? total_kib - host_kib[0] : 0u;
        free_kib = free_kib > held_kib ? free_kib - held_kib : 0u;
        if (host_kib[0] < free_kib) free_kib = host_kib[0];
    }
    const size_t answer[2] = {free_kib, free_kib};
    const ClInfoRequest asked(value_bytes, value, size_ret);
    if (value_bytes < sizeof answer) return asked.give(answer, sizeof answer[0]);
    return asked.give(answer, sizeof answer);
}

cl_int ClIcdRoot::host_device_info(ClientTransport &wire, ClDevice &device, cl_device_info name,
                                   size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::host_device_info");
    const ClInfoRequest asked(value_bytes, value, size_ret);
    const uint64_t      host = device.object()->wire();
    const uint64_t      key = reinterpret_cast<uintptr_t>(device.object());
    cl_int              kept = CL_SUCCESS;
    const bool          volatile_answer = name == free_memory_amd;
    if (!volatile_answer && device_answers_.answer(key, name, asked, kept)) return kept;
    uint8_t      answer[host_extensions_bytes];
    size_t       answered = 0u;
    const cl_int fetched = av::clproto::av_clGetDeviceInfo(wire, host, name, sizeof answer, answer, &answered);
    if (fetched == CL_INVALID_VALUE)
        return av::clproto::av_clGetDeviceInfo(wire, host, name, value_bytes, value, size_ret);
    if (fetched != CL_SUCCESS) return fetched;
    if (!volatile_answer) device_answers_.keep(key, name, answer, answered);
    return asked.give(answer, answered);
}

cl_int ClIcdRoot::host_extensions(ClDevice &device, ExtensionWords &words)
{
    AV_SCOPE("ClIcdRoot::host_extensions");
    ClientTransport *const wire = wire_of(*device.object());
    if (wire == nullptr) return CL_INVALID_DEVICE;
    char         host[host_extensions_bytes] = {};
    size_t       answered = 0u;
    const cl_int asked = host_device_info(*wire, device, CL_DEVICE_EXTENSIONS, sizeof host, host, &answered);
    if (asked != CL_SUCCESS) return asked;
    if (words.hold(host, answered) != Status::ok) return CL_OUT_OF_HOST_MEMORY;
    cl_device_fp_config half = 0u;
    if (!words.holds("cl_khr_fp16")) {
        if (host_device_info(*wire, device, CL_DEVICE_HALF_FP_CONFIG, sizeof half, &half, nullptr) != CL_SUCCESS) {
            half = 0u;
            device_answers_.keep(reinterpret_cast<uintptr_t>(device.object()), CL_DEVICE_HALF_FP_CONFIG, &half, sizeof half);
        }
        if (half != 0u && words.add("cl_khr_fp16") != Status::ok) return CL_OUT_OF_HOST_MEMORY;
    }
    return CL_SUCCESS;
}

cl_int ClIcdRoot::device_extensions(ClDevice &device, size_t value_bytes, void *value,
                                    size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::device_extensions");
    ExtensionWords host(memory_, Pool::paged);
    const cl_int   asked = host_extensions(device, host);
    if (asked != CL_SUCCESS) return asked;
    const size_t needed = device.extensions().report(host, nullptr, 0u);
    if (size_ret != nullptr) *size_ret = needed;
    if (value == nullptr) return CL_SUCCESS;
    if (value_bytes < needed) return CL_INVALID_VALUE;
    static_cast<void>(device.extensions().report(host, static_cast<char *>(value), value_bytes));
    return CL_SUCCESS;
}

cl_int ClIcdRoot::import_handle_types(ClDevice &device, size_t value_bytes, void *value,
                                      size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::import_handle_types");
    ExtensionWords host(memory_, Pool::paged);
    const cl_int   asked = host_extensions(device, host);
    if (asked != CL_SUCCESS) return asked;
    if (!device.extensions().reports(host, "cl_khr_external_memory")) return CL_INVALID_VALUE;
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

cl_uint ClIcdRoot::reference_count(const ClObject &held)
{
    AV_SCOPE("ClIcdRoot::reference_count");
    const ThreadLockHold locked(lock_);
    return held.references();
}

void ClIcdRoot::give_back_to_host(ClientTransport &wire, ClObject::Kind kind, uint64_t handle)
{
    AV_SCOPE("ClIcdRoot::give_back_to_host");
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
    default: break;
    }
}

void *ClIcdRoot::wrap(ClientTransport &transport, ClObject::Kind kind, uint64_t wire,
                      GuestClContext *context, ClObject *parent, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::wrap");
    ClObject *object = nullptr;
    if (objects_.make(kind, wire, context, parent, object) != Status::ok) {
        give_back_to_host(transport, kind, wire);
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    if (kind == ClObject::Kind::event && parent->kind() == ClObject::Kind::command_queue)
        object->stamp(parent->take_ordinal());
    report_error(error, CL_SUCCESS);
    return object;
}

template <typename Property>
cl_int ClIcdRoot::judge(const Property *properties, const uint64_t *allowed, uint32_t allowed_count,
                        ClPropertyList &read) const
{
    AV_SCOPE("ClIcdRoot::judge");
    if (!read.read(properties)) return CL_INVALID_PROPERTY;
    if (read.repeated()) return CL_INVALID_PROPERTY;
    for (uint32_t index = 0u; index < read.pairs(); ++index) {
        const uint64_t name = read.name_at(index);
        bool           known = false;
        for (uint32_t row = 0u; row < allowed_count && !known; ++row) known = allowed[row] == name;
        if (known) continue;
        LogLine(lines_).text("a property this ICD does not take: ").hex(name, 4u).write();
        return CL_INVALID_PROPERTY;
    }
    return CL_SUCCESS;
}

cl_context ClIcdRoot::create_context(const cl_context_properties *properties, const cl_device_id *devices,
                                     cl_uint num_devices, cl_int *error, const ClContextNotify &notify)
{
    AV_SCOPE("ClIcdRoot::create_context");
    if (!notify.valid()) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
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
    const ThreadLockHold locked(lock_);
    GuestClContext *context = nullptr;
    Status          made =
        contexts_.create(context, device->connection(), memory_, send_lock_, blocks_lock_, lines_, shared_handles_);
    if (made == Status::ok) made = context->make(gl_share, watcher_.wake_handle());
    if (made == Status::ok) context->keep_properties(properties);
    if (made == Status::ok) context->notify_with(notify);
    if (made != Status::ok) {
        const bool exhausted = context != nullptr && context->exhausted();
        if (context != nullptr) static_cast<void>(contexts_.destroy(context));
        report_error(error, exhausted                  ? CL_OUT_OF_RESOURCES
                            : made == Status::no_memory ? CL_OUT_OF_HOST_MEMORY
                            : gl_share != 0u            ? CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR
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
    if (wire != nullptr && av::clproto::av_clGetPlatformIDs(*wire, 1u, &host_platform, &platforms) == CL_SUCCESS &&
        av::clproto::av_clGetDeviceIDs(*wire, host_platform, CL_DEVICE_TYPE_GPU, 1u, &host_device,
                                        &found) == CL_SUCCESS) {
        host_context = av::clproto::av_clCreateContext(*wire, nullptr, 1u, &host_device,
                                                        &host_error);
    }
    if (host_context == 0u) {
        const bool exhausted = context->exhausted();
        static_cast<void>(contexts_.destroy(context));
        report_error(error, exhausted                  ? CL_OUT_OF_RESOURCES
                            : host_error != CL_SUCCESS ? host_error
                                                       : CL_DEVICE_NOT_AVAILABLE);
        return nullptr;
    }
    void *const wrapped = wrap(*wire, ClObject::Kind::context, host_context, context, nullptr, error);
    if (wrapped == nullptr) static_cast<void>(contexts_.destroy(context));
    if (wrapped != nullptr && d3d11_device != nullptr) {
        context->d3d11().share_with(d3d11_device);
        d3d11_.hold(d3d11_device);
    }
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
    const ThreadLockHold locked(lock_);
    ClObject *const      held = judged(object, kind);
    if (held == nullptr) return refusal_for(kind);
    held->retain();
    return CL_SUCCESS;
}

cl_int ClIcdRoot::release(void *object, ClObject::Kind kind)
{
    AV_SCOPE("ClIcdRoot::release");
    ClDestructorCallbacks::Due due(memory_);
    ClObject                  *sliced_from = nullptr;
    {
        const ThreadLockHold locked(lock_);
        ClObject *const      held = judged(object, kind);
        if (held == nullptr) return refusal_for(kind);
        sliced_from = drop_reference(object, *held, kind, due);
    }
    if (due.lost() != 0u) refused_.failed("a destructor callback, for want of memory", CL_OUT_OF_HOST_MEMORY);
    due.run();
    if (sliced_from != nullptr) static_cast<void>(release(sliced_from, ClObject::Kind::mem));
    return CL_SUCCESS;
}

ClObject *ClIcdRoot::drop_reference(void *object, ClObject &held, ClObject::Kind kind, ClDestructorCallbacks::Due &due)
{
    AV_SCOPE("ClIcdRoot::drop_reference");
    GuestClContext *const context = held.context();
    const uint64_t        wire_handle = held.wire();
    const bool            last = held.references() == 1u;
    ClObject *const       sliced_from = last ? held.sliced_from() : nullptr;
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
                if (!held.known_idle()) context->orphan_work();
                uint64_t       markers[ClPendingReads::max_reads] = {};
                uint64_t       scratches[ClPendingReads::max_reads] = {};
                uint32_t       scratched = 0u;
                const uint32_t forgotten = reads_.forget_queue(wire_handle, markers, scratches, &scratched);
                for (uint32_t index = 0u; index < forgotten; ++index)
                    static_cast<void>(av::clproto::av_clReleaseEvent(*wire, markers[index]));
                icd::UnansweredTransport quiet(*wire);
                for (uint32_t index = 0u; index < scratched; ++index)
                    static_cast<void>(av::clproto::av_clReleaseMemObject(quiet, scratches[index]));
                give_back_to_host(*wire, kind, wire_handle);
                break;
            }
            case ClObject::Kind::mem:
                refusals_.note_released_mapped(context->give_back_mappings_of(wire_handle));
                if (context->external().give_back(*wire, wire_handle)) break;
                give_back_to_host(*wire, kind, wire_handle);
                if (held.sliced_from() == nullptr && (held.backed() || held.lent() != nullptr))
                    let_go_of_memory(*context, held);
                break;
            default:
                give_back_to_host(*wire, kind, wire_handle);
                break;
            }
        }
    }
    if (twin != nullptr) d3d11_.let_go(twin);
    if (last && kind == ClObject::Kind::program && held.source() != nullptr) {
        static_cast<void>(sources_.destroy(held.source()));
        held.keep_source(nullptr);
    }
    if (last && kind == ClObject::Kind::kernel && held.shape() != nullptr) {
        static_cast<void>(shapes_.destroy(held.shape()));
        held.keep_shape(nullptr);
    }
    GuestClContext *freed = nullptr;
    const void     *gone[ClDestructorCallbacks::max_gone] = {};
    uint32_t        went = 0u;
    static_cast<void>(objects_.release(held, freed, gone, ClDestructorCallbacks::max_gone, went));
    if (freed != nullptr) {
        watcher_.forget_context(freed->wire());
        if (freed->d3d11().device() != nullptr) d3d11_.let_go(freed->d3d11().device());
        static_cast<void>(contexts_.destroy(freed));
    }
    destructor_callbacks_.take(gone, went, due);
    return sliced_from;
}

void ClIcdRoot::report(StateWriter &writer)
{
    AV_SCOPE("ClIcdRoot::report");
    const ReportGate gate(lock_, writer, "avcl");
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
    return platform_.dispatchable();
}

cl_command_queue ClIcdRoot::create_queue(cl_context context, cl_device_id device,
                                         const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_queue");
    static constexpr uint64_t allowed[] = {static_cast<uint64_t>(CL_QUEUE_PROPERTIES),
                                           static_cast<uint64_t>(CL_QUEUE_SIZE)};
    ClPropertyList            list;
    const cl_int              judged_list = judge(properties, allowed, 2u, list);
    if (judged_list != CL_SUCCESS) {
        report_error(error, judged_list == CL_INVALID_PROPERTY ? CL_INVALID_VALUE : judged_list);
        return nullptr;
    }
    ClObject *const held = judged(context, ClObject::Kind::context);
    if (held == nullptr) {
        report_error(error, CL_INVALID_CONTEXT);
        return nullptr;
    }
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
    const ClMemFlags asked(flags);
    const cl_int     judged_flags = asked.judge();
    if (judged_flags != CL_SUCCESS) {
        report_error(error, judged_flags);
        return nullptr;
    }
    if (ClObject *const owner = judged(context, ClObject::Kind::context)) {
        constexpr size_t always_allowed = size_t{32} << 20u;
        const uint64_t   limit = bytes > always_allowed ? max_allocation(*owner) : 0u;
        if (bytes != 0u && limit != 0u && bytes > limit) {
            report_error(error, CL_INVALID_BUFFER_SIZE);
            return nullptr;
        }
    }
    cl_mem const made = create_buffer_for_host(context, asked.for_host(), bytes, host_pointer, properties, error);
    if (made == nullptr) return nullptr;
    ClObject *const object = static_cast<ClObject *>(static_cast<void *>(made));
    object->keep_flags(asked.asked());
    object->extent().as_buffer(0u, bytes);
    return made;
}

cl_mem ClIcdRoot::create_buffer_for_host(cl_context context, cl_mem_flags flags, size_t bytes,
                                         void *host_pointer, const uint64_t *properties, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_buffer_for_host");
    static constexpr uint64_t allowed[] = {
        ClExternalMemory::handle_properties[0], ClExternalMemory::handle_properties[1],
        ClExternalMemory::handle_properties[2], ClExternalMemory::handle_properties[3],
        ClExternalMemory::handle_properties[4], ClPropertyList::device_handle_list};
    static_assert(sizeof allowed / sizeof allowed[0] == ClExternalMemory::handle_property_count + 1u);
    ClPropertyList list;
    const cl_int   judged_list = judge(properties, allowed, ClExternalMemory::handle_property_count + 1u, list);
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
    GuestClContext *const on = held->context();
    if (on->external().asked_for(list)) {
        uint64_t     opened = 0u;
        const cl_int taken = on->external().open(*wire, on->guest_device(), list, bytes, opened);
        if (taken != CL_SUCCESS) {
            report_error(error, taken);
            return nullptr;
        }
        ClObject *object = nullptr;
        if (objects_.make(ClObject::Kind::mem, opened, on, held, object) != Status::ok) {
            static_cast<void>(on->external().give_back(*wire, opened));
            report_error(error, CL_OUT_OF_HOST_MEMORY);
            return nullptr;
        }
        report_error(error, CL_SUCCESS);
        return static_cast<cl_mem>(static_cast<void *>(object));
    }
    const bool uses_application =
        (flags & static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR)) != 0u;
    if (uses_application &&
        (host_pointer == nullptr ||
         (flags & static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR | CL_MEM_COPY_HOST_PTR)) != 0u)) {
        report_error(error, host_pointer == nullptr ? CL_INVALID_HOST_PTR : CL_INVALID_VALUE);
        return nullptr;
    }
    if (uses_application && bytes >= lent_minimum) {
        cl_mem const over = create_over_application(*held, *wire, flags, bytes, host_pointer, error);
        if (over != nullptr) return over;
    }
    if (uses_application)
        flags = (flags & ~static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR)) |
                static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR);
    const bool            wants_host_memory =
        (flags & static_cast<cl_mem_flags>(CL_MEM_ALLOC_HOST_PTR)) != 0u &&
        (flags & static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR | CL_MEM_COPY_HOST_PTR)) == 0u;
    uint64_t block = 0u;
    bool     backed = false;
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
            on->blocks().give_back(block);
            backed = false;
            host_error = CL_SUCCESS;
        }
    }
    const bool copies_after = (flags & static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR)) != 0u &&
                              host_pointer != nullptr && bytes > wire->room().inline_write();
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
        if (backed) on->blocks().give_back(block);
        return nullptr;
    }
    if (backed) static_cast<ClObject *>(made)->back_with(block, bytes);
    if (uses_application) static_cast<ClObject *>(made)->use_application_memory(host_pointer);
    return static_cast<cl_mem>(made);
}

cl_mem ClIcdRoot::create_over_application(ClObject &context, ClientTransport &wire, cl_mem_flags flags,
                                          size_t bytes, void *host_pointer, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_over_application");
    constexpr uintptr_t page = 4096u;
    const uintptr_t     first = reinterpret_cast<uintptr_t>(host_pointer) & ~(page - 1u);
    const uintptr_t     end = (reinterpret_cast<uintptr_t>(host_pointer) + bytes + page - 1u) & ~(page - 1u);
    GuestClContext *const on = context.context();
    HostAllocation       *lent = nullptr;
    if (on->lend(reinterpret_cast<void *>(first), end - first, lent) != Status::ok) return nullptr;
    ClBlobBacking backing;
    backing.set_in(lent->handle(), reinterpret_cast<uintptr_t>(host_pointer) - first);
    uint64_t named[ClBlobBacking::list_words] = {};
    static_cast<void>(backing.write(named));
    cl_int         host_error = CL_SUCCESS;
    const uint64_t buffer = av::clproto::av_clCreateBufferWithProperties(
        wire, context.wire(), named, flags & ~static_cast<cl_mem_flags>(CL_MEM_USE_HOST_PTR), bytes, nullptr,
        &host_error);
    if (buffer == 0u) {
        on->give_back_lent(*lent);
        return nullptr;
    }
    void *const made = wrap(wire, ClObject::Kind::mem, buffer, on, &context, error);
    if (made == nullptr) {
        on->give_back_lent(*lent);
        return nullptr;
    }
    static_cast<ClObject *>(made)->use_application_memory(host_pointer);
    static_cast<ClObject *>(made)->lend(*lent);
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
    void *const made = wrap(*wire, ClObject::Kind::program, program, held->context(), held, error);
    if (made == nullptr) return nullptr;
    ClObject *const object = static_cast<ClObject *>(made);
    object->keep_traits(ClObject::program_from_source);
    bool includes = false;
    for (cl_uint index = 0u; index < count && !includes; ++index) {
        const char *const text = sources[index];
        const size_t      bytes = text == nullptr ? 0u : lengths != nullptr && lengths[index] != 0u ? lengths[index] : ~size_t{0};
        for (size_t at = 0u; text != nullptr && at < bytes && text[at] != '\0' && !includes; ++at)
            includes = text[at] == '#' && at + 8u < bytes && __builtin_memcmp(text + at, "#include", 8u) == 0;
    }
    if (includes) {
        ClProgramSource *kept = nullptr;
        if (sources_.create(kept, memory_) == Status::ok) {
            if (kept->keep_text(sources, lengths, count)) object->keep_source(kept);
            else static_cast<void>(sources_.destroy(kept));
        }
    }
    return static_cast<cl_program>(made);
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
    if ((held->traits() & ClObject::program_failed) != 0u) {
        report_error(error, CL_INVALID_PROGRAM_EXECUTABLE);
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
    void *const made = wrap(*wire, ClObject::Kind::kernel, kernel, held->context(), held, error);
    if (made != nullptr) static_cast<void>(kernel_shape(*static_cast<ClObject *>(made), *wire));
    return static_cast<cl_kernel>(made);
}

cl_int ClIcdRoot::set_kernel_argument(cl_kernel kernel, cl_uint index, size_t bytes,
                                      const void *value)
{
    AV_SCOPE("ClIcdRoot::set_kernel_argument");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClKernelShape *const shape = kernel_shape(*held, *wire);
    if (shape != nullptr) {
        const cl_int judged_argument = judge_kernel_argument(*shape, index, bytes, value);
        if (judged_argument != CL_SUCCESS) return judged_argument;
    }
    icd::UnansweredTransport unanswered(*wire);
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
            const cl_int   sent = av::clproto::av_clSetKernelArg(unanswered, held->wire(), index, sizeof handle,
                                                                 &handle);
            if (sent == CL_SUCCESS && shape != nullptr) shape->note_set(index, bytes, value);
            return sent;
        }
    }
    const cl_int sent = av::clproto::av_clSetKernelArg(unanswered, held->wire(), index, bytes, value);
    if (sent == CL_SUCCESS && shape != nullptr) shape->note_set(index, bytes, value);
    return sent;
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
    if (named->context() != held->context()) return CL_INVALID_CONTEXT;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    const ClKernelShape *const shape = kernel_shape(*named, *wire);
    if (shape != nullptr) {
        const ClObject *const owner = named->parent();
        const bool            non_uniform = owner != nullptr && (owner->traits() & ClObject::program_non_uniform) != 0u;
        const cl_int          judged_launch = shape->judge_launch(dimensions, offset, global, local, non_uniform);
        if (judged_launch != CL_SUCCESS) return judged_launch;
    }
    uint64_t     produced = 0u;
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
        if (context_of(*named) != context) return CL_INVALID_CONTEXT;
        if (!context->external().answered(named->wire())) return CL_INVALID_MEM_OBJECT;
    }
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    if (event == nullptr && waits.count() == 0u) return CL_SUCCESS;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
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

namespace {
constexpr size_t snapshot_limit = size_t{16} << 20u;
}

cl_int ClIcdRoot::enqueue_read(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                               size_t offset, size_t bytes, void *into, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_read");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int allowed = judge_host_transfer(*held, *named, true);
        if (allowed != CL_SUCCESS) return allowed;
    }
    {
        const cl_int in_range = judge_buffer_range(*named, offset, bytes);
        if (in_range != CL_SUCCESS) return in_range;
    }
    if (into == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t     produced = 0u;
    bool has_room = false;
    {
        const ThreadLockHold locked(lock_);
        has_room = reads_.has_room();
    }
    if (blocking != CL_TRUE && bytes <= snapshot_limit && has_room) {
        held->busy();
        cl_int         made = CL_SUCCESS;
        const uint64_t scratch = av::clproto::av_clCreateBuffer(*wire, held->nearest(ClObject::Kind::context)->wire(),
                                                                CL_MEM_READ_WRITE, bytes, nullptr, &made);
        if (scratch != 0u) {
            icd::UnansweredTransport unanswered(*wire);
            const cl_int copied = av::clproto::av_clEnqueueCopyBuffer(
                unanswered, held->wire(), named->wire(), scratch, offset, 0u, bytes, waits.count(),
                waits.count() != 0u ? listed : nullptr, nullptr);
            const cl_int marked = copied == CL_SUCCESS ? av::clproto::av_clEnqueueMarkerWithWaitList(
                                                             *wire, held->wire(), 0u, nullptr, &produced)
                                                       : copied;
            bool recorded = false;
            if (marked == CL_SUCCESS) {
                const ThreadLockHold locked(lock_);
                recorded = reads_.record(produced, held->wire(), scratch, 0u, bytes, into, event == nullptr, scratch);
            }
            if (recorded) {
                if (event != nullptr) {
                    void *const made_event =
                        wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr);
                    if (made_event != nullptr)
                        static_cast<ClObject *>(made_event)->keep_command_type(CL_COMMAND_READ_BUFFER);
                    *event = static_cast<cl_event>(made_event);
                }
                return CL_SUCCESS;
            }
            if (marked == CL_SUCCESS) static_cast<void>(av::clproto::av_clReleaseEvent(*wire, produced));
            give_back_to_host(*wire, ClObject::Kind::mem, scratch);
            produced = 0u;
        }
    }
    if (blocking != CL_TRUE && !has_room) lines_.write("too many reads outstanding; this one blocks");
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     ran = chunks.read(named->wire(), offset, bytes, into);
    if (ran != CL_SUCCESS) return ran;
    return marker_event(*wire, *held, event, CL_COMMAND_READ_BUFFER);
}

cl_int ClIcdRoot::enqueue_write(cl_command_queue queue, cl_mem buffer, cl_bool blocking,
                                size_t offset, size_t bytes, const void *from, cl_event *event,
                                const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_write");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int allowed = judge_host_transfer(*held, *named, false);
        if (allowed != CL_SUCCESS) return allowed;
    }
    {
        const cl_int in_range = judge_buffer_range(*named, offset, bytes);
        if (in_range != CL_SUCCESS) return in_range;
    }
    if (from == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (blocking != CL_TRUE && waits.count() != 0u && bytes <= wire->room().inline_write()) {
        ClHostNumbers listed_room(memory_);
        uint64_t     *listed = nullptr;
        const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
        if (judged_list != CL_SUCCESS) return judged_list;
        held->busy();
        uint64_t     produced = 0u;
        const cl_int sent = av::clproto::av_clEnqueueWriteBuffer(*wire, held->wire(), named->wire(), CL_FALSE, offset,
                                                                  bytes, from, waits.count(), listed, &produced);
        if (sent != CL_SUCCESS) return sent;
        if (event != nullptr) {
            void *const made = wrap(*wire, ClObject::Kind::event, produced, held->context(), held, nullptr);
            if (made != nullptr) static_cast<ClObject *>(made)->keep_command_type(CL_COMMAND_WRITE_BUFFER);
            *event = static_cast<cl_event>(made);
        } else {
            static_cast<void>(av::clproto::av_clReleaseEvent(*wire, produced));
        }
        return CL_SUCCESS;
    }
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     ran = chunks.write(named->wire(), offset, bytes, from);
    if (ran != CL_SUCCESS) return ran;
    return marker_event(*wire, *held, event, CL_COMMAND_WRITE_BUFFER);
}

cl_int ClIcdRoot::finish(cl_command_queue queue)
{
    AV_SCOPE("ClIcdRoot::finish");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int waited = drain_unless_idle(*wire, *held);
    if (waited != CL_SUCCESS) return waited;
    const cl_int delivered = deliver_queued(*wire, context_of(*held)->staging(), held->wire());
    if (delivered != CL_SUCCESS) return delivered;
    return context_of(*held)->flush() == TransportResult::ok ? CL_SUCCESS : CL_OUT_OF_RESOURCES;
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
    if (wire_of(*held) == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
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
    {
        const cl_int allowed = judge_host_map(*held, *named, flags);
        if (allowed != CL_SUCCESS) {
            report_error(error, allowed);
            return nullptr;
        }
        const cl_int in_range = judge_buffer_range(*named, offset, bytes);
        if (in_range != CL_SUCCESS) {
            report_error(error, in_range);
            return nullptr;
        }
    }
    GuestClContext *const context = held->context();
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    if (waits.count() != 0u || waits.events() != nullptr) {
        const cl_int settled = settle(*wire, *held, waits);
        if (settled != CL_SUCCESS) {
            report_error(error, settled);
            return nullptr;
        }
    }
    if (named->lent() != nullptr) {
        const uintptr_t own = reinterpret_cast<uintptr_t>(named->application_memory());
        const uintptr_t lent_end =
            reinterpret_cast<uintptr_t>(named->lent()->view()) + static_cast<uintptr_t>(named->lent()->bytes());
        if (offset > lent_end - own || bytes > lent_end - own - offset) {
            report_error(error, CL_INVALID_VALUE);
            return nullptr;
        }
    }
    const bool zero_copy = named->lent() != nullptr || (named->backed() && offset <= named->backing_bytes() &&
                                                         bytes <= named->backing_bytes() - offset);
    if (zero_copy) {
        uint8_t *shared = named->application_memory();
        if (named->lent() == nullptr) {
            const ClObject *const owner = named->sliced_from() != nullptr ? named->sliced_from() : named;
            uint8_t *const        block = context->blocks().at(owner->backing_offset(), owner->backing_bytes());
            if (block == nullptr) {
                report_error(error, CL_INVALID_MEM_OBJECT);
                return nullptr;
            }
            shared = block + (named->backing_offset() - owner->backing_offset());
        }
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
            if (context->take_shared_mapping(named->wire(), host_mapped, offset, bytes, at) ==
                nullptr) {
                refusals_.note_map("clEnqueueMapBuffer: no mapping slot for an arena map", bytes,
                                   context->mappings());
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
    uint8_t *const      own = named->application_memory();
    MappedRegion *const taken = own != nullptr ? context->take_application_mapping(named->wire(), mapped, offset,
                                                                                    bytes, flags, own + offset)
                                               : context->take_mapping(named->wire(), mapped, offset, bytes, flags);
    if (taken == nullptr) {
        refusals_.note_map("clEnqueueMapBuffer: no slot or no staging block for a map", bytes, context->mappings());
        static_cast<void>(
            av::clproto::av_clEnqueueUnmapMemObject(*wire, held->wire(), named->wire(), mapped, 0u, nullptr, nullptr));
        report_error(error, CL_OUT_OF_HOST_MEMORY);
        return nullptr;
    }
    if ((flags & CL_MAP_WRITE_INVALIDATE_REGION) == 0u) {
        ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
        const cl_int     read = chunks.read(named->wire(), offset, bytes, taken->at());
        if (read != CL_SUCCESS) {
            context->give_back_mapping(*taken);
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
    return taken->at();
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
    MappedRegion *const region = context->mapping_at(named->wire(), pointer);
    if (region == nullptr) return CL_INVALID_VALUE;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
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
    cl_int written = CL_SUCCESS;
    if (unmapped == CL_SUCCESS && region->writes_back()) {
        ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
        written = chunks.write(named->wire(), static_cast<size_t>(region->offset()),
                               static_cast<size_t>(region->bytes()), region->at());
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
    constexpr cl_svm_mem_flags device_access = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY;
    constexpr cl_svm_mem_flags known = device_access | CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS;
    if ((flags & ~known) != 0u || (flags & (CL_MEM_SVM_FINE_GRAIN_BUFFER | CL_MEM_SVM_ATOMICS)) != 0u) return nullptr;
    const cl_svm_mem_flags access = flags & device_access;
    if ((access & (access - 1u)) != 0u) return nullptr;
    if (alignment != 0u && (alignment & (alignment - 1u)) != 0u) return nullptr;
    GuestClContext *const owner = held->context();
    ClientTransport *const wire = owner != nullptr ? owner->wire() : nullptr;
    if (wire == nullptr) return nullptr;
    uint64_t host = av::clproto::av_clSVMAlloc(*wire, held->wire(), flags, bytes, alignment);
    if (host == 0u && alignment != 0u) host = av::clproto::av_clSVMAlloc(*wire, held->wire(), flags, bytes, 0u);
    if (host == 0u) return nullptr;
    void *const mirror = owner->svm().take(host, bytes, alignment);
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
    const uint64_t         host = context->svm().host_address_of(pointer);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
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
    const uint64_t offset = static_cast<uint64_t>(static_cast<const uint8_t *>(pointer) - allocation->mirror());
    const bool     reads = (flags & CL_MAP_WRITE_INVALIDATE_REGION) == 0u;
    if (reads && allocation->take_device_write()) {
        const cl_int settled = settle(*wire, *held, ClWaitList());
        if (settled != CL_SUCCESS) return settled;
        const cl_int copied =
            svm_to_guest(*wire, *held, const_cast<void *>(pointer), context->svm().host_address_of(pointer), bytes);
        if (copied != CL_SUCCESS) return copied;
    }
    allocation->note_region(offset, bytes, (flags & (CL_MAP_WRITE | CL_MAP_WRITE_INVALIDATE_REGION)) != 0u);
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
    const uint64_t         host = context->svm().host_address_of(pointer);
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*context, waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    {
        const uint64_t offset = static_cast<uint64_t>(static_cast<const uint8_t *>(pointer) - allocation->mirror());
        uint64_t       bytes = 0u;
        bool           writes = false;
        if (allocation->take_region(offset, bytes, writes) && writes) {
            const cl_int settled = settle(*wire, *held, ClWaitList());
            if (settled != CL_SUCCESS) return settled;
            const cl_int copied = svm_from_guest(*wire, *held, host, pointer, static_cast<size_t>(bytes));
            if (copied != CL_SUCCESS) return copied;
            const cl_int landed = drain_unless_idle(*wire, *held);
            if (landed != CL_SUCCESS) return landed;
        }
    }
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
    const uint64_t host = context->svm().host_address_of(pointer);
    if (host == 0u) return CL_INVALID_ARG_VALUE;
    ClientTransport *const wire = context->wire();
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClKernelShape *const shape = kernel_shape(*held, *wire);
    if (shape != nullptr && index >= shape->count()) return CL_INVALID_ARG_INDEX;
    context->svm().covering(pointer)->note_device_write();
    const cl_int sent = av::clproto::av_clSetKernelArgSVMPointer(*wire, held->wire(), index, host);
    if (sent == CL_SUCCESS && shape != nullptr) shape->note_set_pointer(index);
    return sent;
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
    if (held->parent() == nullptr || held->parent()->kind() != ClObject::Kind::context) return CL_INVALID_EVENT;
    if (status > 0 && status != CL_COMPLETE) return CL_INVALID_VALUE;
    if ((held->traits() & ClObject::event_settled) != 0u) return CL_INVALID_OPERATION;
    const cl_int set = av::clproto::av_clSetUserEventStatus(*wire, held->wire(), status);
    if (set == CL_SUCCESS) held->keep_traits(held->traits() | ClObject::event_settled);
    return set;
}

cl_int ClIcdRoot::wait_for_events(cl_uint count, const cl_event *events)
{
    AV_SCOPE("ClIcdRoot::wait_for_events");
    if (count == 0u || events == nullptr) return CL_INVALID_VALUE;
    ClHostNumbers   handles_room(memory_);
    uint64_t *const handles = handles_room.room(count);
    if (handles == nullptr) return CL_OUT_OF_HOST_MEMORY;
    GuestClContext *context = nullptr;
    const cl_uint   waited = count;
    for (cl_uint index = 0u; index < waited; ++index) {
        ClObject *const held = judged(events[index], ClObject::Kind::event);
        if (held == nullptr) return CL_INVALID_EVENT;
        if (context == nullptr) context = held->context();
        if (held->context() != context) return CL_INVALID_CONTEXT;
        handles[index] = held->wire();
    }
    ClientTransport *const wire = context != nullptr ? context->wire() : nullptr;
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int reached = waits_.until_complete(*wire, handles, waited, icd::SlicedWait::forever, context->wakes());
    const cl_int delivered = deliver_reads(*wire, context->staging(), handles, waited);
    return reached != CL_SUCCESS ? reached : delivered;
}

cl_int ClIcdRoot::event_info(cl_event event, cl_event_info name, size_t value_bytes, void *value,
                             size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::event_info");
    ClObject *const held = judged(event, ClObject::Kind::event);
    if (held == nullptr) return CL_INVALID_EVENT;
    {
        const ClInfoRequest guest_asked(value_bytes, value, size_ret);
        if (name == CL_EVENT_COMMAND_QUEUE) {
            const ClObject *const owner = held->parent();
            const cl_command_queue queue = owner != nullptr && owner->kind() == ClObject::Kind::command_queue
                                               ? reinterpret_cast<cl_command_queue>(const_cast<ClObject *>(owner))
                                               : nullptr;
            return guest_asked.give(&queue, sizeof queue);
        }
        if (name == CL_EVENT_CONTEXT) {
            const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
            return guest_asked.give(&owner, sizeof owner);
        }
        if (name == CL_EVENT_COMMAND_TYPE && held->command_type() != 0u) {
            const cl_command_type type = held->command_type();
            return guest_asked.give(&type, sizeof type);
        }
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int asked =
        av::clproto::av_clGetEventInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
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
    refused_.unserved(entry_point);
}

cl_int ClIcdRoot::noted(const char *entry_point, cl_int result)
{
    AV_SCOPE("ClIcdRoot::noted");
    if (result < 0) refused_.failed(entry_point, result);
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
    const ClInfoRequest asked(value_bytes, value, size_ret);
    switch (name) {
    case CL_CONTEXT_REFERENCE_COUNT: {
        const cl_uint count = reference_count(*held);
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
        const cl_uint count = reference_count(*held);
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
        const cl_uint count = reference_count(*held);
        return asked.give(&count, sizeof count);
    }
    case CL_MEM_HOST_PTR: {
        const void *const own = held->application_memory();
        return asked.give(&own, sizeof own);
    }
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
        made |= held->flags() &
                static_cast<cl_mem_flags>(CL_MEM_HOST_WRITE_ONLY | CL_MEM_HOST_READ_ONLY | CL_MEM_HOST_NO_ACCESS);
        return asked.give(&made, sizeof made);
    }
    default:
        break;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return host_mem_info(*wire, *held, name, value_bytes, value, size_ret);
}

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
    if (name == CL_IMAGE_BUFFER) {
        const cl_mem named = held->companion() != nullptr && judged(held->companion(), ClObject::Kind::mem) != nullptr
                                 ? reinterpret_cast<cl_mem>(held->companion())
                                 : nullptr;
        return ClInfoRequest(value_bytes, value, size_ret).give(&named, sizeof named);
    }
    if ((name == CL_IMAGE_ROW_PITCH || name == CL_IMAGE_SLICE_PITCH) && held->extent().shape() == ClMemExtent::Shape::image &&
        held->extent().element_bytes() != 0u &&
        (held->companion() != nullptr || held->extent().type() == CL_MEM_OBJECT_IMAGE1D_BUFFER)) {
        const size_t pitch = name == CL_IMAGE_ROW_PITCH ? held->extent().row_pitch() : held->extent().slice_pitch();
        return ClInfoRequest(value_bytes, value, size_ret).give(&pitch, sizeof pitch);
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
    const cl_int told = av::clproto::av_clSetEventCallback(*wire, held->wire(), status);
    if (told != CL_SUCCESS) return told;
    {
        const ThreadLockHold locked(lock_);
        held->retain();
    }
    const cl_int registered = watcher_.watch(held, held->wire(), wire, status, notify, user_data, context->wakes());
    if (registered != CL_SUCCESS) static_cast<void>(release(held, ClObject::Kind::event));
    return registered;
}


cl_int ClIcdRoot::deliver_reads(ClientTransport &wire, ClArenaBlocks &blocks, const uint64_t *events, uint32_t count)
{
    AV_SCOPE("ClIcdRoot::deliver_reads");
    cl_int answer = CL_SUCCESS;
    for (uint32_t index = 0u; index < count; ++index) {
        const ThreadLockHold locked(lock_);
        ClPendingRead *const waiting = reads_.waiting_on(events[index]);
        if (waiting == nullptr) continue;
        ClTransferChunks chunks(wire, waiting->queue(), blocks);
        const cl_int     read =
            chunks.read(waiting->buffer(), waiting->offset(), waiting->bytes(), waiting->into());
        if (waiting->ours())
            static_cast<void>(av::clproto::av_clReleaseEvent(wire, waiting->event()));
        if (waiting->scratch() != 0u) {
            icd::UnansweredTransport quiet(wire);
            static_cast<void>(av::clproto::av_clReleaseMemObject(quiet, waiting->scratch()));
        }
        waiting->give_up();
        reads_.count_delivery();
        if (read != CL_SUCCESS && answer == CL_SUCCESS) answer = read;
    }
    return answer;
}

cl_int ClIcdRoot::deliver_queued(ClientTransport &wire, ClArenaBlocks &blocks, uint64_t queue)
{
    AV_SCOPE("ClIcdRoot::deliver_queued");
    cl_int answer = CL_SUCCESS;
    for (uint32_t taken = 0u; taken < ClPendingReads::max_reads; ++taken) {
        uint64_t event = 0u;
        {
            const ThreadLockHold locked(lock_);
            for (uint32_t slot = 0u; slot < ClPendingReads::max_reads && event == 0u; ++slot) {
                const ClPendingRead *const row = reads_.at(slot);
                if (row->held() && row->queue() == queue) event = row->event();
            }
        }
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

cl_int ClIcdRoot::drain_unless_idle(ClientTransport &wire, ClObject &queue)
{
    AV_SCOPE("ClIcdRoot::drain_unless_idle");
    if (queue.known_idle()) return CL_SUCCESS;
    const cl_int drained = drain(wire, queue.wire(), queue.context()->wakes(), nullptr, 0u);
    if (drained == CL_SUCCESS) drained_queue(queue);
    return drained;
}

void ClIcdRoot::drained_queue(ClObject &queue)
{
    AV_SCOPE("ClIcdRoot::drained_queue");
    queue.idle();
    if (objects_.queues_idle(queue.nearest(ClObject::Kind::context))) queue.context()->reclaim();
}

void ClIcdRoot::let_go_of_memory(GuestClContext &context, ClObject &buffer)
{
    AV_SCOPE("ClIcdRoot::let_go_of_memory");
    const uint64_t block = buffer.backing_offset();
    if (!objects_.queues_idle(buffer.nearest(ClObject::Kind::context))) {
        context.retire(buffer.backed() ? &block : nullptr, buffer.lent());
        return;
    }
    if (buffer.backed()) context.blocks().give_back(block);
    if (buffer.lent() != nullptr) context.give_back_lent(*buffer.lent());
}

cl_int ClIcdRoot::marker_event(ClientTransport &wire, ClObject &queue, cl_event *event, cl_command_type type)
{
    AV_SCOPE("ClIcdRoot::marker_event");
    if (event == nullptr) return CL_SUCCESS;
    uint64_t     produced = 0u;
    const cl_int marked =
        av::clproto::av_clEnqueueMarkerWithWaitList(wire, queue.wire(), 0u, nullptr, &produced);
    if (marked != CL_SUCCESS) return marked;
    void *const made = wrap(wire, ClObject::Kind::event, produced, queue.context(), &queue, nullptr);
    if (made != nullptr) static_cast<ClObject *>(made)->keep_command_type(type);
    *event = static_cast<cl_event>(made);
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
    ClObject *const named_device = judged(device, ClObject::Kind::device);
    const uint64_t  host_device = host_device_of(*held);
    if (named_device == nullptr || host_device == 0u || named_device->wire() != host_device ||
        devices_.of(*named_device) != devices_.running(*held->context()))
        return CL_INVALID_DEVICE;
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    if (name == CL_PROGRAM_BUILD_OPTIONS && held->source() != nullptr && held->source()->options() != nullptr) {
        const char *const kept_options = held->source()->options();
        size_t            kept_length = 0u;
        while (kept_options[kept_length] != '\0') ++kept_length;
        return ClInfoRequest(value_bytes, value, size_ret).give(kept_options, kept_length + 1u);
    }
    const bool appended = name == CL_PROGRAM_BUILD_OPTIONS && (held->traits() & ClObject::program_typed_arguments) != 0u &&
                          (held->traits() & ClObject::program_arguments_asked) == 0u;
    if (!appended)
        return av::clproto::av_clGetProgramBuildInfo(*wire, held->wire(), host_device, name, value_bytes, value,
                                                      size_ret);
    size_t size = 0u;
    const cl_int sized =
        av::clproto::av_clGetProgramBuildInfo(*wire, held->wire(), host_device, name, 0u, nullptr, &size);
    if (sized != CL_SUCCESS) return sized;
    char *const text = static_cast<char *>(memory_.allocate(size + 1u, Pool::paged));
    if (text == nullptr) return CL_OUT_OF_HOST_MEMORY;
    ScopeExit given_back([&] { memory_.release(text); });
    text[0] = '\0';
    if (size != 0u) {
        const cl_int read =
            av::clproto::av_clGetProgramBuildInfo(*wire, held->wire(), host_device, name, size, text, nullptr);
        if (read != CL_SUCCESS) return read;
    }
    text[size] = '\0';
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    length = ClBuildOptions(memory_, nullptr).without_arguments_option(text, length);
    return ClInfoRequest(value_bytes, value, size_ret).give(text, length + 1u);
}

cl_int ClIcdRoot::kernel_work_group_info(cl_kernel kernel, cl_device_id device,
                                         cl_kernel_work_group_info name, size_t value_bytes,
                                         void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::kernel_work_group_info");
    ClObject *const held = judged(kernel, ClObject::Kind::kernel);
    if (held == nullptr) return CL_INVALID_KERNEL;
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
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const source = judged(from, ClObject::Kind::mem);
    ClObject *const target = judged(to, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (source->context() != held->context() || target->context() != held->context()) return CL_INVALID_CONTEXT;
    {
        const cl_int from_range = judge_buffer_range(*source, from_offset, bytes);
        if (from_range != CL_SUCCESS) return from_range;
        const cl_int to_range = judge_buffer_range(*target, to_offset, bytes);
        if (to_range != CL_SUCCESS) return to_range;
        if (buffer_ranges_overlap(*source, from_offset, *target, to_offset, bytes)) return CL_MEM_COPY_OVERLAP;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
    if (judged_list != CL_SUCCESS) return judged_list;
    uint64_t     produced = 0u;
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
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    if (pattern == nullptr || pattern_bytes == 0u) return CL_INVALID_VALUE;
    if (named->context() != held->context()) return CL_INVALID_CONTEXT;
    {
        const cl_int judged_fill = judge_fill(*named, pattern_bytes, offset, bytes);
        if (judged_fill != CL_SUCCESS) return judged_fill;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
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
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const source = judged(from, ClObject::Kind::mem);
    ClObject *const target = judged(to, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from_origin == nullptr || to_origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    if (source->context() != held->context() || target->context() != held->context()) return CL_INVALID_CONTEXT;
    if (source->extent().shape() == ClMemExtent::Shape::buffer ||
        target->extent().shape() == ClMemExtent::Shape::buffer)
        return CL_INVALID_MEM_OBJECT;
    {
        if (!source->extent().same_format(target->extent())) return CL_IMAGE_FORMAT_MISMATCH;
        const cl_int from_region = source->extent().judge_region(from_origin, region);
        if (from_region != CL_SUCCESS) return from_region;
        const cl_int to_region = target->extent().judge_region(to_origin, region);
        if (to_region != CL_SUCCESS) return to_region;
        if (source == target && source->extent().regions_overlap(from_origin, target->extent(), to_origin, region))
            return CL_MEM_COPY_OVERLAP;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
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

ClImageLimits ClIcdRoot::image_limits(ClObject &context, bool dimensions, bool pitch)
{
    AV_SCOPE("ClIcdRoot::image_limits");
    ClImageLimits limits = {};
    ClientTransport *const wire = wire_of(context);
    ClDevice *const        device = context.context() != nullptr ? devices_.running(*context.context()) : nullptr;
    if (wire == nullptr || device == nullptr) return limits;
    size_t sizes[7] = {};
    const cl_device_info names[7] = {CL_DEVICE_IMAGE2D_MAX_WIDTH, CL_DEVICE_IMAGE2D_MAX_HEIGHT, CL_DEVICE_IMAGE3D_MAX_WIDTH,
                                     CL_DEVICE_IMAGE3D_MAX_HEIGHT, CL_DEVICE_IMAGE3D_MAX_DEPTH, CL_DEVICE_IMAGE_MAX_ARRAY_SIZE,
                                     CL_DEVICE_IMAGE_MAX_BUFFER_SIZE};
    for (uint32_t index = 0u; index < 7u && dimensions; ++index) {
        size_t value = 0u;
        if (host_device_info(*wire, *device, names[index], sizeof value, &value, nullptr) == CL_SUCCESS) sizes[index] = value;
    }
    limits.width_2d_ = sizes[0];
    limits.height_2d_ = sizes[1];
    limits.width_3d_ = sizes[2];
    limits.height_3d_ = sizes[3];
    limits.depth_3d_ = sizes[4];
    limits.array_size_ = sizes[5];
    limits.buffer_size_ = sizes[6];
    cl_uint alignment = 0u;
    if (pitch && host_device_info(*wire, *device, CL_DEVICE_IMAGE_PITCH_ALIGNMENT, sizeof alignment, &alignment, nullptr) ==
                     CL_SUCCESS)
        limits.pitch_alignment_ = alignment;
    return limits;
}

uint64_t ClIcdRoot::max_allocation(ClObject &context)
{
    AV_SCOPE("ClIcdRoot::max_allocation");
    ClientTransport *const wire = wire_of(context);
    ClDevice *const        device = context.context() != nullptr ? devices_.running(*context.context()) : nullptr;
    cl_ulong               bytes = 0u;
    if (wire == nullptr || device == nullptr ||
        host_device_info(*wire, *device, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof bytes, &bytes, nullptr) != CL_SUCCESS)
        return 0u;
    return bytes;
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
    const ClMemFlags asked_flags(flags);
    const cl_int     judged_flags = asked_flags.judge();
    if (judged_flags != CL_SUCCESS) {
        report_error(error, judged_flags);
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
        lines_.write("an image over the application's own memory is not carried");
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
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
        described.buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(buffer->wire()));
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) {
        report_error(error, CL_OUT_OF_RESOURCES);
        return nullptr;
    }
    {
        ClMemExtent shape;
        shape.as_image(desc->image_type, 1u, 1u, 1u, 1u, *format);
        const ClObject *const backing = desc->buffer != nullptr ? judged(desc->buffer, ClObject::Kind::mem) : nullptr;
        const bool            large = desc->image_width > 8192u || desc->image_height > 8192u || desc->image_depth > 2048u ||
                           desc->image_array_size > 2048u || desc->image_width > 65536u;
        const cl_int          judged_image = ClImageJudge(image_limits(*held, large, desc->buffer != nullptr && desc->image_row_pitch != 0u)).judge(
            *desc, shape.element_bytes(), desc->buffer != nullptr, backing != nullptr ? backing->extent().bytes() : 0u,
            host_pointer != nullptr);
        if (judged_image != CL_SUCCESS) {
            report_error(error, judged_image);
            return nullptr;
        }
    }
    cl_int         host_error = CL_SUCCESS;
    const uint64_t image = av::clproto::av_clCreateImage(
        *wire, held->wire(),
        desc->buffer != nullptr ? 0u : asked_flags.for_host() & ~static_cast<cl_mem_flags>(CL_MEM_COPY_HOST_PTR), format,
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
    void *const made = wrap(*wire, ClObject::Kind::mem, image, held->context(), held, error);
    if (made != nullptr) {
        ClObject *const object = static_cast<ClObject *>(made);
        object->keep_flags(asked_flags.asked());
        if (desc->buffer != nullptr) object->keep_companion(judged(desc->buffer, ClObject::Kind::mem));
        object->extent().as_image(desc->image_type, desc->image_width, desc->image_height, desc->image_depth,
                                  desc->image_array_size, *format);
        object->extent().with_pitches(desc->image_row_pitch, desc->image_slice_pitch);
    }
    return static_cast<cl_mem>(made);
}

cl_int ClIcdRoot::enqueue_read_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                                     const size_t *region, size_t row_pitch, size_t slice_pitch,
                                     void *into, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_read_image");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(image, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int allowed = judge_host_transfer(*held, *named, true);
        if (allowed != CL_SUCCESS) return allowed;
    }
    if (origin == nullptr || region == nullptr || into == nullptr) return CL_INVALID_VALUE;
    if (named->extent().shape() == ClMemExtent::Shape::buffer) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int in_region = named->extent().judge_region(origin, region);
        if (in_region != CL_SUCCESS) return in_region;
    }
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
    return marker_event(*wire, *held, event, CL_COMMAND_READ_IMAGE);
}

cl_int ClIcdRoot::enqueue_write_image(cl_command_queue queue, cl_mem image, const size_t *origin,
                                      const size_t *region, size_t row_pitch, size_t slice_pitch,
                                      const void *from, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_write_image");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(image, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int allowed = judge_host_transfer(*held, *named, false);
        if (allowed != CL_SUCCESS) return allowed;
    }
    if (origin == nullptr || region == nullptr || from == nullptr) return CL_INVALID_VALUE;
    if (named->extent().shape() == ClMemExtent::Shape::buffer) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int in_region = named->extent().judge_region(origin, region);
        if (in_region != CL_SUCCESS) return in_region;
    }
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
    return marker_event(*wire, *held, event, CL_COMMAND_WRITE_IMAGE);
}

cl_int ClIcdRoot::enqueue_read_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                           void *into, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_read_buffer_rect");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int allowed = judge_host_transfer(*held, *named, true);
        if (allowed != CL_SUCCESS) return allowed;
    }
    if (into == nullptr) return CL_INVALID_VALUE;
    const cl_int checked = rect.check();
    if (checked != CL_SUCCESS) return checked;
    {
        const cl_int in_range = judge_rect_transfer(*named, rect);
        if (in_range != CL_SUCCESS) return in_range;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     read = chunks.read_buffer_rect(named->wire(), rect, into);
    if (read != CL_SUCCESS) return read;
    return marker_event(*wire, *held, event, CL_COMMAND_READ_BUFFER_RECT);
}

cl_int ClIcdRoot::enqueue_write_buffer_rect(cl_command_queue queue, cl_mem buffer, const ClBufferRect &rect,
                                            const void *from, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_write_buffer_rect");
    ClObject *const held = judged(queue, ClObject::Kind::command_queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const named = judged(buffer, ClObject::Kind::mem);
    if (named == nullptr) return CL_INVALID_MEM_OBJECT;
    {
        const cl_int allowed = judge_host_transfer(*held, *named, false);
        if (allowed != CL_SUCCESS) return allowed;
    }
    if (from == nullptr) return CL_INVALID_VALUE;
    const cl_int checked = rect.check();
    if (checked != CL_SUCCESS) return checked;
    {
        const cl_int in_range = judge_rect_transfer(*named, rect);
        if (in_range != CL_SUCCESS) return in_range;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    const cl_int drained = settle(*wire, *held, waits);
    if (drained != CL_SUCCESS) return drained;
    ClTransferChunks chunks(*wire, held->wire(), context_of(*held)->staging());
    const cl_int     written = chunks.write_buffer_rect(named->wire(), rect, from);
    if (written != CL_SUCCESS) return written;
    return marker_event(*wire, *held, event, CL_COMMAND_WRITE_BUFFER_RECT);
}

cl_int ClIcdRoot::enqueue_copy_buffer_rect(cl_command_queue queue, cl_mem from, cl_mem to,
                                           const size_t *from_origin, const size_t *to_origin, const size_t *region,
                                           size_t from_row_pitch, size_t from_slice_pitch, size_t to_row_pitch,
                                           size_t to_slice_pitch, cl_event *event, const ClWaitList &waits)
{
    AV_SCOPE("ClIcdRoot::enqueue_copy_buffer_rect");
    ClObject *const held = busied(queue);
    if (held == nullptr) return CL_INVALID_COMMAND_QUEUE;
    if (wire_of(*held) == nullptr) return CL_OUT_OF_RESOURCES;
    ClObject *const source = judged(from, ClObject::Kind::mem);
    ClObject *const target = judged(to, ClObject::Kind::mem);
    if (source == nullptr || target == nullptr) return CL_INVALID_MEM_OBJECT;
    if (from_origin == nullptr || to_origin == nullptr || region == nullptr) return CL_INVALID_VALUE;
    if (source->context() != held->context() || target->context() != held->context()) return CL_INVALID_CONTEXT;
    {
        const cl_int judged_rect = judge_rect_copy(*source, *target, from_origin, to_origin, region, from_row_pitch,
                                                   from_slice_pitch, to_row_pitch, to_slice_pitch);
        if (judged_rect != CL_SUCCESS) return judged_rect;
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    ClHostNumbers listed_room(memory_);
    uint64_t     *listed = nullptr;
    const cl_int judged_list = wait_list(*held->context(), waits.count(), waits.events(), listed_room, listed);
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

}

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
    constexpr cl_mem_flags access = CL_MEM_READ_WRITE | CL_MEM_WRITE_ONLY | CL_MEM_READ_ONLY | CL_MEM_KERNEL_READ_AND_WRITE;
    const cl_mem_flags     key = flags & access;
    GuestClContext *const  owner = held->context();
    uint32_t               count = 0u;
    const cl_image_format *known = owner->formats().find(key, type, count);
    if (known == nullptr) {
        cl_image_format all[ClFormatCache::most_formats] = {};
        cl_uint         listed = 0u;
        const cl_int    asked = av::clproto::av_clGetSupportedImageFormats(*wire, held->wire(), flags, type,
                                                                           ClFormatCache::most_formats, all, sizeof all, &listed);
        if (asked != CL_SUCCESS || listed > ClFormatCache::most_formats)
            return av::clproto::av_clGetSupportedImageFormats(*wire, held->wire(), flags, type, num_entries, formats,
                                                              formats != nullptr ? num_entries * sizeof(cl_image_format) : 0u,
                                                              num_formats);
        cl_image_format usable[ClFormatCache::most_formats] = {};
        uint32_t        kept = 0u;
        uint64_t        backing = 0u;
        cl_int          made = CL_SUCCESS;
        if (type == CL_MEM_OBJECT_IMAGE1D_BUFFER)
            backing = av::clproto::av_clCreateBuffer(*wire, held->wire(), CL_MEM_READ_WRITE, 4096u, nullptr, &made);
        for (cl_uint index = 0u; index < listed; ++index) {
            cl_image_desc desc = {};
            desc.image_type = type;
            desc.image_width = 1u;
            desc.image_height = 1u;
            desc.image_depth = 1u;
            desc.image_array_size = 1u;
            desc.buffer = reinterpret_cast<cl_mem>(static_cast<uintptr_t>(backing));
            cl_int         refused = CL_SUCCESS;
            const uint64_t probe = (type == CL_MEM_OBJECT_IMAGE1D_BUFFER && backing == 0u)
                                       ? 1u
                                       : av::clproto::av_clCreateImage(*wire, held->wire(),
                                                                       type == CL_MEM_OBJECT_IMAGE1D_BUFFER ? cl_mem_flags{0} : key & ~static_cast<cl_mem_flags>(CL_MEM_KERNEL_READ_AND_WRITE),
                                                                       &all[index], &desc, nullptr, 0u, &refused);
            if (probe != 0u && !(type == CL_MEM_OBJECT_IMAGE1D_BUFFER && backing == 0u)) give_back_to_host(*wire, ClObject::Kind::mem, probe);
            if (probe != 0u || refused != CL_IMAGE_FORMAT_NOT_SUPPORTED) usable[kept++] = all[index];
        }
        if (backing != 0u) give_back_to_host(*wire, ClObject::Kind::mem, backing);
        static_cast<void>(owner->formats().keep(key, type, usable, kept));
        known = owner->formats().find(key, type, count);
        if (known == nullptr) {
            count = kept;
            known = usable;
            if (num_formats != nullptr) *num_formats = kept;
            if (formats != nullptr)
                for (uint32_t index = 0u; index < kept && index < num_entries; ++index) formats[index] = usable[index];
            return CL_SUCCESS;
        }
    }
    if (num_formats != nullptr) *num_formats = count;
    if (formats != nullptr)
        for (uint32_t index = 0u; index < count && index < num_entries; ++index) formats[index] = known[index];
    return CL_SUCCESS;
}

cl_int ClIcdRoot::wait_list(GuestClContext &context, cl_uint count, const cl_event *events, ClHostNumbers &room,
                            uint64_t *&into)
{
    AV_SCOPE("ClIcdRoot::wait_list");
    if ((count == 0u) != (events == nullptr)) return CL_INVALID_EVENT_WAIT_LIST;
    into = room.room(count);
    if (into == nullptr) return CL_OUT_OF_HOST_MEMORY;
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
    ClHostNumbers handles_room(memory_);
    uint64_t     *handles = nullptr;
    const cl_int listed = wait_list(*context, count, events, handles_room, handles);
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
    void *const object = wrap(wire, ClObject::Kind::mem, made, context.context(), &context, error);
    if (object != nullptr) static_cast<ClObject *>(object)->keep_traits(ClObject::mem_from_gl);
    return static_cast<cl_mem>(object);
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
    ClDevice *device = nullptr;
    Status    opened = Status::not_found;
    bool      exhausted = false;
    {
        const ThreadLockHold locked(lock_);
        learn_devices();
        device = devices_.on(luid);
        if (device != nullptr) opened = open_device(*device, exhausted);
    }
    if (opened != Status::ok) {
        if (num_devices != nullptr) *num_devices = 0u;
        return exhausted ? CL_OUT_OF_RESOURCES : CL_DEVICE_NOT_FOUND;
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
    gl_share_.run_held_calls();
    cl_int           host_error = CL_SUCCESS;
    const uint64_t   made = av::clproto::av_clCreateFromGLBuffer(wire, held->wire(), flags, buffer, &host_error);
    return gl_object(wire, *held, made, host_error, error);
}

bool ClIcdRoot::texture_target(cl_GLenum target)
{
    AV_SCOPE("ClIcdRoot::texture_target");
    switch (target) {
    case 0x0DE0u: case 0x0DE1u: case 0x806Fu: case 0x8C18u: case 0x8C1Au: case 0x8C2Au: case 0x84F5u:
    case 0x8515u: case 0x8516u: case 0x8517u: case 0x8518u: case 0x8519u: case 0x851Au: return true;
    default: return false;
    }
}

cl_mem ClIcdRoot::create_from_gl_texture(cl_context context, cl_mem_flags flags, cl_GLenum target, cl_GLint level,
                                         cl_GLuint texture, cl_int *error)
{
    AV_SCOPE("ClIcdRoot::create_from_gl_texture");
    ClObject *const held = gl_sharing(context, error);
    if (held == nullptr) return nullptr;
    if (!texture_target(target)) {
        report_error(error, CL_INVALID_VALUE);
        return nullptr;
    }
    ClientTransport &wire = *wire_of(*held);
    gl_share_.run_held_calls();
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
    gl_share_.run_held_calls();
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
    if ((count == 0u) != (objects == nullptr)) return CL_INVALID_VALUE;
    ClHostNumbers   named_room(memory_);
    uint64_t *const named = named_room.room(count);
    if (named == nullptr) return CL_OUT_OF_HOST_MEMORY;
    for (cl_uint index = 0u; index < count; index++) {
        ClObject *const object = judged(objects[index], ClObject::Kind::mem);
        if (object == nullptr) return CL_INVALID_MEM_OBJECT;
        if (context_of(*object) != context) return CL_INVALID_CONTEXT;
        if ((object->traits() & ClObject::mem_from_gl) == 0u) return CL_INVALID_GL_OBJECT;
        named[index] = object->wire();
    }
    ClHostNumbers handles_room(memory_);
    uint64_t     *handles = nullptr;
    const cl_int listed = wait_list(*context, waits, events, handles_room, handles);
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
    ClDevice *device = nullptr;
    bool      opened = false;
    uint64_t  gl_context = 0u;
    static_cast<void>(list.names(static_cast<uint64_t>(CL_GL_CONTEXT_KHR), gl_context));
    const uint64_t gl_luid = gl_share_.adapter_luid(reinterpret_cast<void *>(static_cast<uintptr_t>(gl_context)));
    {
        const ThreadLockHold locked(lock_);
        learn_devices();
        device = devices_.on(gl_luid);
        if (device == nullptr) device = devices_.count() != 0u ? &devices_.at(0u) : nullptr;
        bool refused = false;
        opened = device != nullptr && open_device(*device, refused) == Status::ok;
    }
    if (!opened) return CL_OUT_OF_RESOURCES;
    ExtensionWords host(memory_, Pool::paged);
    const cl_int   listed = host_extensions(*device, host);
    if (listed != CL_SUCCESS) return listed;
    const bool shares = device->extensions().reports(host, "cl_khr_gl_sharing");
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

}
