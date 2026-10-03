#include "icd/opencl/cl_device_report.hpp"

#include "core/trace/function_scope.hpp"

#include <CL/cl_ext.h>

namespace av::clicd {

namespace {

/* The version this ICD implements. A host that answers more is capped to it,
 * because what is reported is what is tested (constitution VI). */
constexpr uint32_t implemented_major = 3u;
constexpr uint32_t implemented_minor = 0u;

/* The one grain of SVM feature 052 mirrors. */
constexpr cl_device_svm_capabilities carried_svm = CL_DEVICE_SVM_COARSE_GRAIN_BUFFER;

constexpr const char *device_prefix = "OpenCL ";
constexpr const char *language_prefix = "OpenCL C ";
/* What follows the number, so a log of an application's device list says which
 * driver answered. */
constexpr const char *reported_by = " avgpu";

size_t length_within(const char *text, size_t bytes)
{
    AV_SCOPE("length_within");
    size_t length = 0u;
    while (length < bytes && text[length] != '\0') ++length;
    return length;
}

bool digit(char letter)
{
    AV_SCOPE("digit");
    return letter >= '0' && letter <= '9';
}

bool at_most_implemented(uint32_t major, uint32_t minor)
{
    AV_SCOPE("at_most_implemented");
    if (major > implemented_major) return false;
    return major < implemented_major || minor <= implemented_minor;
}

/* "<prefix><major>.<minor>" followed by a space or by nothing, which is the
 * shape the specification fixes for both version strings. A number of more
 * than one digit is no OpenCL release, and reading only its first digit would
 * turn 12.0 into 1.0. */
bool read_version(const char *host, size_t host_bytes, const char *prefix, uint32_t &major,
                  uint32_t &minor)
{
    AV_SCOPE("read_version");
    if (host == nullptr) return false;
    const size_t length = length_within(host, host_bytes);
    /* No terminator in the bytes the host gave: whatever follows is somebody
     * else's memory, not the rest of a version. */
    if (length == host_bytes) return false;
    size_t at = 0u;
    for (; prefix[at] != '\0'; ++at) {
        if (at >= length || host[at] != prefix[at]) return false;
    }
    if (length - at < 3u) return false;
    if (!digit(host[at]) || host[at + 1u] != '.' || !digit(host[at + 2u])) return false;
    if (length - at > 3u && host[at + 3u] != ' ') return false;
    major = static_cast<uint32_t>(host[at] - '0');
    minor = static_cast<uint32_t>(host[at + 2u] - '0');
    return true;
}

void write_version(char *into, const char *prefix, uint32_t major, uint32_t minor)
{
    AV_SCOPE("write_version");
    size_t at = 0u;
    for (size_t index = 0u; prefix[index] != '\0'; ++index) into[at++] = prefix[index];
    into[at++] = static_cast<char>('0' + static_cast<char>(major));
    into[at++] = '.';
    into[at++] = static_cast<char>('0' + static_cast<char>(minor));
    for (size_t index = 0u; reported_by[index] != '\0'; ++index) into[at++] = reported_by[index];
    into[at] = '\0';
}

bool built_within(const char *text, size_t room)
{
    AV_SCOPE("built_within");
    if (text[0] == '\0') return false;
    return length_within(text, room) < room;
}

}  // namespace

ClDeviceReport::ClDeviceReport(uint64_t adapter_luid)
    : luid_(adapter_luid), version_{}, c_version_{}, major_(implemented_major),
      minor_(implemented_minor), c_major_(implemented_major), c_minor_(implemented_minor), svm_(0u)
{
    AV_SCOPE("ClDeviceReport::ClDeviceReport");
    write_version(version_, device_prefix, major_, minor_);
    write_version(c_version_, language_prefix, c_major_, c_minor_);
}

Status ClDeviceReport::observe_version(const char *host, size_t host_bytes)
{
    AV_SCOPE("ClDeviceReport::observe_version");
    uint32_t major = 0u;
    uint32_t minor = 0u;
    if (!read_version(host, host_bytes, device_prefix, major, minor)) return Status::malformed;
    if (!at_most_implemented(major, minor)) {
        major = implemented_major;
        minor = implemented_minor;
    }
    major_ = major;
    minor_ = minor;
    write_version(version_, device_prefix, major_, minor_);
    return Status::ok;
}

Status ClDeviceReport::observe_c_version(const char *host, size_t host_bytes)
{
    AV_SCOPE("ClDeviceReport::observe_c_version");
    uint32_t major = 0u;
    uint32_t minor = 0u;
    if (!read_version(host, host_bytes, language_prefix, major, minor)) return Status::malformed;
    if (!at_most_implemented(major, minor)) {
        major = implemented_major;
        minor = implemented_minor;
    }
    c_major_ = major;
    c_minor_ = minor;
    write_version(c_version_, language_prefix, c_major_, c_minor_);
    return Status::ok;
}

void ClDeviceReport::observe_svm(cl_device_svm_capabilities host)
{
    AV_SCOPE("ClDeviceReport::observe_svm");
    svm_ = host & carried_svm;
}

bool ClDeviceReport::decides(cl_device_info name_asked) const
{
    AV_SCOPE("ClDeviceReport::decides");
    /* One list, not two: what it decides is exactly what it can answer, and a
     * size query with no buffer is that question asked of info(). */
    return info(name_asked, 0u, nullptr, nullptr) == CL_SUCCESS;
}

cl_int ClDeviceReport::answer(const void *from, size_t bytes, size_t value_bytes, void *value,
                              size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer");
    if (value != nullptr) {
        /* Too small is an error and writes nothing: a caller that asked for
         * the size first and then shrank its buffer gets told, rather than a
         * LUID cut where it happened to fit and matching another adapter. */
        if (value_bytes < bytes) return CL_INVALID_VALUE;
        const char *const source = static_cast<const char *>(from);
        char *const       out    = static_cast<char *>(value);
        for (size_t index = 0u; index < bytes; ++index) out[index] = source[index];
    }
    if (size_ret != nullptr) *size_ret = bytes;
    return CL_SUCCESS;
}

cl_int ClDeviceReport::answer_text(const char *text, size_t value_bytes, void *value,
                                   size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::answer_text");
    /* One byte short of the room, because the last of it is the terminator's:
     * a string that somehow lost its is still answered from inside itself. */
    return answer(text, length_within(text, version_bytes - 1u) + 1u, value_bytes, value, size_ret);
}

cl_int ClDeviceReport::info(cl_device_info name_asked, size_t value_bytes, void *value,
                            size_t *size_ret) const
{
    AV_SCOPE("ClDeviceReport::info");
    switch (name_asked) {
    case CL_DEVICE_VENDOR_ID: {
        const cl_uint reported = vendor_id;
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_LUID_KHR: {
        /* LowPart then HighPart: the 64-bit value's bytes little-endian, which
         * is what an application memcmps against a DXGI adapter's LUID. */
        cl_uchar reported[CL_LUID_SIZE_KHR];
        for (size_t index = 0u; index < sizeof reported; ++index) {
            reported[index] = static_cast<cl_uchar>((luid_ >> (index * 8u)) & 0xFFu);
        }
        return answer(reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_LUID_VALID_KHR: {
        /* Zero is what Kmt::adapter_luid leaves when it found no adapter, and
         * is no adapter's LUID. */
        const cl_bool reported = luid_ != 0u ? CL_TRUE : CL_FALSE;
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_VERSION: return answer_text(version_, value_bytes, value, size_ret);
    case CL_DEVICE_OPENCL_C_VERSION: return answer_text(c_version_, value_bytes, value, size_ret);
    case CL_DEVICE_NUMERIC_VERSION: {
        const cl_version reported = CL_MAKE_VERSION(major_, minor_, 0u);
        return answer(&reported, sizeof reported, value_bytes, value, size_ret);
    }
    case CL_DEVICE_SVM_CAPABILITIES:
        return answer(&svm_, sizeof svm_, value_bytes, value, size_ret);
    default: return CL_INVALID_VALUE;
    }
}

void ClDeviceReport::observe_luid(uint64_t adapter_luid)
{
    AV_SCOPE("ClDeviceReport::observe_luid");
    luid_ = adapter_luid;
}

bool ClDeviceReport::names_adapter(uint64_t adapter_luid) const
{
    AV_SCOPE("ClDeviceReport::names_adapter");
    return luid_ != 0u && luid_ == adapter_luid;
}

bool ClDeviceReport::invariants_hold() const
{
    AV_SCOPE("ClDeviceReport::invariants_hold");
    if (!at_most_implemented(major_, minor_)) return false;
    if (!at_most_implemented(c_major_, c_minor_)) return false;
    if (!built_within(version_, version_bytes)) return false;
    if (!built_within(c_version_, version_bytes)) return false;
    return (svm_ & ~carried_svm) == 0u;
}

}  // namespace av::clicd
