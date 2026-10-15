/* Which host OpenCL runs a GPU when more than one offers it.
 *
 * A host may have the GPU's vendor runtime and Mesa's rusticl both, each a
 * platform with a device whose CL_DEVICE_UUID_KHR is the GPU's - Intel's
 * compute-runtime beside rusticl over iris, ROCm beside rusticl over
 * radeonsi. The vendor's is the one taken: its compiler is the one the
 * vendor's Windows driver ships, so a program a Windows application was
 * written and tested against is judged by the same front end, where an
 * implementation over another driver stack (Mesa's rusticl and Clover,
 * PoCL) accepts and rejects its own set. Between two of a kind, the platform
 * whose name sorts first, so the choice does not hang on the order the ICD
 * loader happened to enumerate in. */
#ifndef AV_CL_PLATFORM_CHOICE_HPP
#define AV_CL_PLATFORM_CHOICE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class ClPlatformChoice {
public:
    /* Longer platform names are compared on this much of them. */
    static constexpr size_t name_bytes = 64u;

    ClPlatformChoice();
    ClPlatformChoice(const ClPlatformChoice &) = delete;
    ClPlatformChoice &operator=(const ClPlatformChoice &) = delete;

    /* `device` of `platform`, which calls itself `name` (CL_PLATFORM_NAME), is
     * the GPU: kept when nothing is, or when it is the better of the two. */
    void consider(void *platform, void *device, const char *name);

    /* The cl_platform_id and cl_device_id kept, or null. */
    void *platform() const;
    void *device() const;

private:
    /* Whether a platform called `name` would be kept over the one kept. */
    bool better(uint32_t rank, const char *name) const;

    void    *platform_;
    void    *device_;
    uint32_t rank_;
    char     name_[name_bytes];
};

}  // namespace av

#endif  // AV_CL_PLATFORM_CHOICE_HPP
