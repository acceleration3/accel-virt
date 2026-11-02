#ifndef AV_CLICD_CL_PLATFORM_HPP
#define AV_CLICD_CL_PLATFORM_HPP

#include "icd/common/version_text.hpp"

#include <CL/cl.h>
#include <CL/cl_icd.h>

#include <stddef.h>

namespace av::clicd {

struct ClPlatformProbe;

class ClPlatform {
public:
    static constexpr const char *suffix = "AV";
    static constexpr const char *base_name = "avgpu OpenCL";
    static constexpr const char *vendor = "accel-virt";
    static constexpr const char *profile = "FULL_PROFILE";

    ClPlatform();
    ClPlatform(const ClPlatform &) = delete;
    ClPlatform &operator=(const ClPlatform &) = delete;

    void dispatch_through(const cl_icd_dispatch &table);
    bool dispatchable() const;

    cl_platform_id as_handle();
    bool           names_this(cl_platform_id platform) const;

    cl_int info(cl_platform_id platform, cl_platform_info name_asked, size_t value_bytes,
                void *value, size_t *size_ret) const;

    void        cap_version(uint32_t major, uint32_t minor);
    void        observe_adapter(const char *adapter_name);
    bool        names_an_adapter() const;

    bool invariants_hold() const;

private:
    friend struct ClPlatformProbe;

    cl_int answer(const char *text, size_t value_bytes, void *value, size_t *size_ret) const;
    size_t length_of(const char *text) const;

    const cl_icd_dispatch *dispatch_;
    char                   version_[32];
    char                   name_[160];
    icd::VersionText       version_number_;
};

}

#endif
