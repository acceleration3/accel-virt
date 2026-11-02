#ifndef AV_NT_STATUS_HPP
#define AV_NT_STATUS_HPP

#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class NtStatus {
public:
    static constexpr uint32_t buffer_too_small = 0xC0000023u;
    static constexpr uint32_t source_not_in_topology = 0xC01E0339u;
    static constexpr uint32_t mode_already_in_mode_set = 0xC01E0314u;
    static constexpr uint32_t invalid_video_present_source_mode = 0xC01E0310u;
    static constexpr uint32_t path_content_geometry_transformation_not_supported = 0xC01E0346u;
    static constexpr uint32_t gamma_ramp_not_supported = 0xC01E0348u;
    static constexpr uint32_t no_recommended_functional_vidpn = 0xC01E0323u;
    static constexpr uint32_t no_recommended_vidpn_topology = 0xC01E031Au;
    static constexpr uint32_t not_implemented = 0xC0000002u;

    explicit NtStatus(Status status);
    explicit NtStatus(uint32_t value);
    explicit NtStatus(int32_t answer);

    int32_t value() const;
    bool    succeeded() const;
    Status  status() const;

private:
    static constexpr uint32_t internal_error = 0xC00000E5u;

    uint32_t value_;
};

}

#endif
