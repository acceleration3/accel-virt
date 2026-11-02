#include "icd/opencl/cl_platform.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

#include <CL/cl_ext.h>

namespace av::clicd {

namespace {
constexpr uint32_t implemented_major = 3u;
constexpr uint32_t implemented_minor = 0u;

}

ClPlatform::ClPlatform()
    : dispatch_(nullptr), version_{}, name_{}, version_number_("OpenCL ", implemented_major, implemented_minor)
{
    AV_SCOPE("ClPlatform::ClPlatform");
    cap_version(implemented_major, implemented_minor);
    TextBuffer text(name_, sizeof name_);
    text.append(base_name);
}

void ClPlatform::observe_adapter(const char *adapter_name)
{
    AV_SCOPE("ClPlatform::observe_adapter");
    if (adapter_name == nullptr || adapter_name[0] == '\0') return;
    TextBuffer text(name_, sizeof name_);
    text.append(base_name);
    text.append(" on ");
    text.append(adapter_name);
}

bool ClPlatform::names_an_adapter() const
{
    AV_SCOPE("ClPlatform::names_an_adapter");
    size_t length = 0u;
    while (base_name[length] != '\0') ++length;
    return name_[length] != '\0';
}

void ClPlatform::dispatch_through(const cl_icd_dispatch &table)
{
    AV_SCOPE("ClPlatform::dispatch_through");
    dispatch_ = &table;
}

bool ClPlatform::dispatchable() const
{
    AV_SCOPE("ClPlatform::dispatchable");
    return dispatch_ != nullptr;
}

cl_platform_id ClPlatform::as_handle()
{
    AV_SCOPE("ClPlatform::as_handle");
    return reinterpret_cast<cl_platform_id>(this);
}

bool ClPlatform::names_this(cl_platform_id platform) const
{
    AV_SCOPE("ClPlatform::names_this");
    return platform == nullptr || platform == reinterpret_cast<const cl_platform_id>(
                                                  const_cast<ClPlatform *>(this));
}

void ClPlatform::cap_version(uint32_t major, uint32_t minor)
{
    AV_SCOPE("ClPlatform::cap_version");
    version_number_.cap(major, minor);
    version_number_.write(version_, sizeof version_, " avgpu");
}

size_t ClPlatform::length_of(const char *text) const
{
    AV_SCOPE("ClPlatform::length_of");
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

cl_int ClPlatform::answer(const char *text, size_t value_bytes, void *value, size_t *size_ret) const
{
    AV_SCOPE("ClPlatform::answer");
    const size_t bytes = length_of(text) + 1u;
    if (value != nullptr) {
        if (value_bytes < bytes) return CL_INVALID_VALUE;
        char *const out = static_cast<char *>(value);
        for (size_t index = 0u; index < bytes; ++index) out[index] = text[index];
    }
    if (size_ret != nullptr) *size_ret = bytes;
    return CL_SUCCESS;
}

cl_int ClPlatform::info(cl_platform_id platform, cl_platform_info name_asked, size_t value_bytes,
                        void *value, size_t *size_ret) const
{
    AV_SCOPE("ClPlatform::info");
    if (!names_this(platform)) return CL_INVALID_PLATFORM;
    switch (name_asked) {
    case CL_PLATFORM_PROFILE: return answer(profile, value_bytes, value, size_ret);
    case CL_PLATFORM_VERSION: return answer(version_, value_bytes, value, size_ret);
    case CL_PLATFORM_NAME: return answer(name_, value_bytes, value, size_ret);
    case CL_PLATFORM_VENDOR: return answer(vendor, value_bytes, value, size_ret);
    case CL_PLATFORM_ICD_SUFFIX_KHR: return answer(suffix, value_bytes, value, size_ret);
    case CL_PLATFORM_NUMERIC_VERSION: {
        if (value != nullptr) {
            if (value_bytes < sizeof(cl_version)) return CL_INVALID_VALUE;
            *static_cast<cl_version *>(value) = CL_MAKE_VERSION(version_number_.major(), version_number_.minor(), 0u);
        }
        if (size_ret != nullptr) *size_ret = sizeof(cl_version);
        return CL_SUCCESS;
    }
    case CL_PLATFORM_HOST_TIMER_RESOLUTION: {
        if (value != nullptr) {
            if (value_bytes < sizeof(cl_ulong)) return CL_INVALID_VALUE;
            *static_cast<cl_ulong *>(value) = 0u;
        }
        if (size_ret != nullptr) *size_ret = sizeof(cl_ulong);
        return CL_SUCCESS;
    }
    default: return CL_INVALID_VALUE;
    }
}

bool ClPlatform::invariants_hold() const
{
    AV_SCOPE("ClPlatform::invariants_hold");
    if (!version_number_.invariants_hold()) return false;
    if (version_[0] == '\0') return false;
    size_t index = 0u;
    while (index < sizeof version_ && version_[index] != '\0') ++index;
    return index < sizeof version_;
}

}
