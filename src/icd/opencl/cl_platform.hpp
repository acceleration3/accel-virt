/* The one platform this ICD shows the Khronos loader.
 *
 * The loader asks two things of a platform while it is deciding whether to
 * keep the driver at all, and both are answered from here with no adapter
 * opened: CL_PLATFORM_EXTENSIONS, and CL_PLATFORM_ICD_SUFFIX_KHR, whose
 * failure drops the platform outright (research §1). Every process that calls
 * clGetPlatformIDs asks them, including processes with no interest in this
 * adapter, so a platform that opened a device here would open one in all of
 * them.
 *
 * The first data member is the dispatch table pointer, because that is the
 * whole of the ICD ABI: the loader reads the first word of whatever it is
 * handed and calls through it. */
#ifndef AV_CLICD_CL_PLATFORM_HPP
#define AV_CLICD_CL_PLATFORM_HPP

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <stddef.h>

namespace av::clicd {

class ClPlatform {
public:
    /* What the loader appends to a suffixed extension function's name to
     * route it here. */
    static constexpr const char *suffix = "RVK";
    static constexpr const char *name = "rvkgpu OpenCL";
    static constexpr const char *vendor = "accel-virt";
    /* A full profile, which is what a platform backed by a real GPU is; the
     * embedded profile is for devices that may leave parts of the language
     * out, and nothing here does. */
    static constexpr const char *profile = "FULL_PROFILE";

    ClPlatform();
    ClPlatform(const ClPlatform &) = delete;
    ClPlatform &operator=(const ClPlatform &) = delete;

    /* Binds the table every object of this ICD carries. Until this, the
     * platform is not one the loader would keep. */
    void dispatch_through(const cl_icd_dispatch &table);
    bool dispatchable() const;

    /* The platform as the loader holds it. */
    cl_platform_id as_handle();
    /* Whether `platform` is this one: null is, because the API lets a caller
     * leave the platform out when there is only one, and this ICD shows one. */
    bool           names_this(cl_platform_id platform) const;

    /* clGetPlatformInfo, in the API's own shape: the value's bytes are
     * written when `value` is not null and `value_bytes` is at least the
     * value's size, and the size is written to `size_ret` when that is not
     * null. CL_INVALID_VALUE for a name this platform does not carry and for
     * a buffer too small; CL_INVALID_PLATFORM for another platform's. */
    cl_int info(cl_platform_id platform, cl_platform_info name_asked, size_t value_bytes,
                void *value, size_t *size_ret) const;

    /* What the version says, which is the host's capped at the version this
     * ICD implements. Until a device says otherwise it is this ICD's own. */
    void        cap_version(uint32_t major, uint32_t minor);
    const char *version() const;

    bool invariants_hold() const;

private:
    /* The one query whose answer is a string this builds rather than holds. */
    cl_int answer(const char *text, size_t value_bytes, void *value, size_t *size_ret) const;

    /* First, and it stays first: the loader reads this word. */
    const cl_icd_dispatch *dispatch_;
    /* "OpenCL <major>.<minor> rvkgpu", built when the version is capped. */
    char                   version_[32];
    uint32_t               major_;
    uint32_t               minor_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_PLATFORM_HPP
