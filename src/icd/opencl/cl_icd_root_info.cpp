/* ClIcdRoot's answers about what the platform and its devices are, and the
 * calls that make or watch an object without crossing to the host for it:
 * a context from a device type, the destructor callbacks, sub-devices no
 * device here can be split into, and a sampler's own answers. */
#include "icd/opencl/cl_icd_root.hpp"

#include "core/support/extension_words.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/thread_lock.hpp"
#include "icd/opencl/cl_extension_policy.hpp"
#include "icd/opencl/cl_info_request.hpp"
#include "proto/generated/opencl/cl_dispatch_client.h"

namespace av::clicd {

namespace {

/* The OpenCL C feature a kernel enqueues work from the device with, which
 * needs the device queue no queue here is. */
constexpr char device_enqueue_feature[] = "__opencl_c_device_enqueue";

bool names_feature(const cl_name_version &entry, const char *feature)
{
    AV_SCOPE("names_feature");
    size_t at = 0u;
    for (; at < CL_NAME_VERSION_MAX_NAME_SIZE && feature[at] != '\0'; ++at) {
        if (entry.name[at] != feature[at]) return false;
    }
    return at < CL_NAME_VERSION_MAX_NAME_SIZE && entry.name[at] == '\0';
}

}  // namespace

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
    /* What CL_PLATFORM_EXTENSIONS names, as platform_info finds it: the rows
     * this ICD answers alone, before any host is asked. */
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
    lock_.acquire();
    const Status kept = destructor_callbacks_.keep(context, callback, user_data);
    lock_.release();
    return kept == Status::ok ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
}

cl_int ClIcdRoot::watch_destruction(cl_mem object, ClDestructorCallbacks::MemCallback callback, void *user_data)
{
    AV_SCOPE("ClIcdRoot::watch_destruction");
    if (judged(object, ClObject::Kind::mem) == nullptr) return CL_INVALID_MEM_OBJECT;
    if (callback == nullptr) return CL_INVALID_VALUE;
    lock_.acquire();
    const Status kept = destructor_callbacks_.keep(object, callback, user_data);
    lock_.release();
    return kept == Status::ok ? CL_SUCCESS : CL_OUT_OF_HOST_MEMORY;
}

cl_int ClIcdRoot::create_sub_devices(cl_device_id device, const cl_device_partition_property *, cl_uint,
                                     cl_device_id *, cl_uint *)
{
    AV_SCOPE("ClIcdRoot::create_sub_devices");
    if (judged(device, ClObject::Kind::device) == nullptr) return CL_INVALID_DEVICE;
    /* The specification's word for a partition the device does not support,
     * which is every one: CL_DEVICE_PARTITION_PROPERTIES says none. */
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
        lock_.acquire();
        const cl_uint count = held->references();
        lock_.release();
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

}  // namespace av::clicd
