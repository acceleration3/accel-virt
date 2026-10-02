#include "icd/opencl/cl_platform.hpp"

#include "core/trace/function_scope.hpp"

#include <CL/cl_ext.h>

namespace av::clicd {

namespace {

/* The version this ICD implements. A host that answers more is capped to it,
 * because what is reported is what is tested (constitution VI). */
constexpr uint32_t implemented_major = 3u;
constexpr uint32_t implemented_minor = 0u;

size_t length_of(const char *text)
{
    AV_SCOPE("length_of");
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

}  // namespace

ClPlatform::ClPlatform() : dispatch_(nullptr), version_{}, major_(implemented_major), minor_(implemented_minor)
{
    AV_SCOPE("ClPlatform::ClPlatform");
    cap_version(implemented_major, implemented_minor);
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
    /* Null names this one: the API lets a caller leave the platform out where
     * there is only one, and the guest is shown only this. */
    return platform == nullptr || platform == reinterpret_cast<const cl_platform_id>(
                                                  const_cast<ClPlatform *>(this));
}

void ClPlatform::cap_version(uint32_t major, uint32_t minor)
{
    AV_SCOPE("ClPlatform::cap_version");
    if (major > implemented_major || (major == implemented_major && minor > implemented_minor)) {
        major = implemented_major;
        minor = implemented_minor;
    }
    major_ = major;
    minor_ = minor;
    /* "OpenCL <major>.<minor> avgpu", which is the shape the specification
     * fixes: the word, a space, the version, a space, and whatever the
     * vendor cares to say. */
    const char prefix[] = "OpenCL ";
    size_t at = 0u;
    for (size_t index = 0u; prefix[index] != '\0'; ++index) version_[at++] = prefix[index];
    version_[at++] = static_cast<char>('0' + static_cast<char>(major_ % 10u));
    version_[at++] = '.';
    version_[at++] = static_cast<char>('0' + static_cast<char>(minor_ % 10u));
    const char tail[] = " avgpu";
    for (size_t index = 0u; tail[index] != '\0'; ++index) version_[at++] = tail[index];
    version_[at] = '\0';
}

cl_int ClPlatform::answer(const char *text, size_t value_bytes, void *value, size_t *size_ret) const
{
    AV_SCOPE("ClPlatform::answer");
    const size_t bytes = length_of(text) + 1u;
    if (value != nullptr) {
        /* Too small is an error and writes nothing: a caller that asked for
         * the size first and then shrank its buffer gets told, rather than a
         * string cut where it happened to fit. */
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
    case CL_PLATFORM_NAME: return answer(name, value_bytes, value, size_ret);
    case CL_PLATFORM_VENDOR: return answer(vendor, value_bytes, value, size_ret);
    /* The one query an ICD may not decline: a platform whose suffix cannot be
     * read is dropped by the loader (research §1). */
    case CL_PLATFORM_ICD_SUFFIX_KHR: return answer(suffix, value_bytes, value, size_ret);
    case CL_PLATFORM_NUMERIC_VERSION: {
        if (value != nullptr) {
            if (value_bytes < sizeof(cl_version)) return CL_INVALID_VALUE;
            *static_cast<cl_version *>(value) = CL_MAKE_VERSION(major_, minor_, 0u);
        }
        if (size_ret != nullptr) *size_ret = sizeof(cl_version);
        return CL_SUCCESS;
    }
    default: return CL_INVALID_VALUE;
    }
}

bool ClPlatform::invariants_hold() const
{
    AV_SCOPE("ClPlatform::invariants_hold");
    if (major_ > implemented_major) return false;
    if (major_ == implemented_major && minor_ > implemented_minor) return false;
    if (version_[0] == '\0') return false;
    /* The version string is terminated inside its room. */
    size_t index = 0u;
    while (index < sizeof version_ && version_[index] != '\0') ++index;
    return index < sizeof version_;
}

}  // namespace av::clicd
