#include "icd/opencl/cl_icd_root.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/platform/thread_lock_hold.hpp"
#include "core/support/extension_words.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_extension_policy.hpp"
#include "icd/opencl/cl_info_request.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {
constexpr char device_enqueue_feature[] = "__opencl_c_device_enqueue";

static_assert(sizeof device_enqueue_feature <= CL_NAME_VERSION_MAX_NAME_SIZE, "a feature name must fit a cl_name_version");

bool names_feature(const cl_name_version &entry, const char *feature)
{
    AV_SCOPE("names_feature");
    size_t at = 0u;
    for (; feature[at] != '\0'; ++at) {
        if (entry.name[at] != feature[at]) return false;
    }
    return entry.name[at] == '\0';
}

}

cl_int ClIcdRoot::device_extension_versions(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::device_extension_versions");
    ExtensionWords host(memory_, Pool::paged);
    const cl_int   asked = host_extensions(device, host);
    if (asked != CL_SUCCESS) return asked;
    cl_name_version listed[ClExtensionPolicy::max_rows] = {};
    const size_t    count = device.extensions().report_versions(host, listed, ClExtensionPolicy::max_rows);
    return ClInfoRequest(value_bytes, value, size_ret).give(listed, count * sizeof listed[0]);
}

cl_int ClIcdRoot::c_features(ClDevice &device, size_t value_bytes, void *value, size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::c_features");
    ClientTransport *const wire = wire_of(*device.object());
    if (wire == nullptr) return CL_INVALID_DEVICE;
    constexpr size_t most = host_extensions_bytes / sizeof(cl_name_version);
    cl_name_version  features[most] = {};
    size_t           answered = 0u;
    const cl_int asked = host_device_info(*wire, device, CL_DEVICE_OPENCL_C_FEATURES, sizeof features, features,
                                          &answered);
    if (asked != CL_SUCCESS) return asked;
    size_t kept = 0u;
    for (size_t index = 0u; index < answered / sizeof features[0]; ++index) {
        if (!names_feature(features[index], device_enqueue_feature)) features[kept++] = features[index];
    }
    return ClInfoRequest(value_bytes, value, size_ret).give(features, kept * sizeof features[0]);
}

cl_int ClIcdRoot::platform_extension_versions(size_t value_bytes, void *value, size_t *size_ret) const
{
    AV_SCOPE("ClIcdRoot::platform_extension_versions");
    const ClExtensionPolicy platform_wide(reported_extensions, reported_extension_count);
    const ExtensionWords    no_host(memory_, Pool::paged);
    cl_name_version         listed[ClExtensionPolicy::max_rows] = {};
    const size_t            count = platform_wide.report_versions(no_host, listed, ClExtensionPolicy::max_rows);
    return ClInfoRequest(value_bytes, value, size_ret).give(listed, count * sizeof listed[0]);
}

cl_context ClIcdRoot::create_context_from_type(const cl_context_properties *properties, cl_device_type type,
                                               cl_int *error, const ClContextNotify &notify)
{
    AV_SCOPE("ClIcdRoot::create_context_from_type");
    cl_device_id device = nullptr;
    cl_uint      found = 0u;
    const cl_int listed = device_ids(platform_.as_handle(), type, 1u, &device, &found);
    if (listed != CL_SUCCESS) {
        if (error != nullptr) *error = listed;
        return nullptr;
    }
    return create_context(properties, &device, 1u, error, notify);
}

cl_int ClIcdRoot::watch_destruction(cl_context context, ClDestructorCallbacks::ContextCallback callback,
                                    void *user_data)
{
    AV_SCOPE("ClIcdRoot::watch_destruction");
    if (judged(context, ClObject::Kind::context) == nullptr) return CL_INVALID_CONTEXT;
    if (callback == nullptr) return CL_INVALID_VALUE;
    const ThreadLockHold locked(lock_);
    return destructor_callbacks_.keep(context, callback, user_data) == Status::ok ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
}

cl_int ClIcdRoot::watch_destruction(cl_mem object, ClDestructorCallbacks::MemCallback callback, void *user_data)
{
    AV_SCOPE("ClIcdRoot::watch_destruction");
    if (judged(object, ClObject::Kind::mem) == nullptr) return CL_INVALID_MEM_OBJECT;
    if (callback == nullptr) return CL_INVALID_VALUE;
    const ThreadLockHold locked(lock_);
    return destructor_callbacks_.keep(object, callback, user_data) == Status::ok ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
}

cl_int ClIcdRoot::create_sub_devices(cl_device_id device, const cl_device_partition_property *, cl_uint,
                                     cl_device_id *, cl_uint *)
{
    AV_SCOPE("ClIcdRoot::create_sub_devices");
    if (judged(device, ClObject::Kind::device) == nullptr) return CL_INVALID_DEVICE;
    return CL_INVALID_VALUE;
}

void *ClIcdRoot::extension_function_for(cl_platform_id platform, const char *name)
{
    AV_SCOPE("ClIcdRoot::extension_function_for");
    return platform != nullptr && platform_.names_this(platform) ? extension_function(name) : nullptr;
}

cl_int ClIcdRoot::sampler_info(cl_sampler sampler, cl_sampler_info name, size_t value_bytes, void *value,
                               size_t *size_ret)
{
    AV_SCOPE("ClIcdRoot::sampler_info");
    ClObject *const held = judged(sampler, ClObject::Kind::sampler);
    if (held == nullptr) return CL_INVALID_SAMPLER;
    const ClInfoRequest asked(value_bytes, value, size_ret);
    if (name == CL_SAMPLER_REFERENCE_COUNT) {
        const cl_uint count = reference_count(*held);
        return asked.give(&count, sizeof count);
    }
    if (name == CL_SAMPLER_CONTEXT) {
        const cl_context owner = reinterpret_cast<cl_context>(held->nearest(ClObject::Kind::context));
        return asked.give(&owner, sizeof owner);
    }
    ClientTransport *const wire = wire_of(*held);
    if (wire == nullptr) return CL_OUT_OF_RESOURCES;
    return av::clproto::av_clGetSamplerInfo(*wire, held->wire(), name, value_bytes, value, size_ret);
}

}
