/* A core Status carried across the DDI boundary as the NTSTATUS dxgkrnl
 * expects, plus the few graphics statuses core has no name for. */
#ifndef AV_NT_STATUS_HPP
#define AV_NT_STATUS_HPP

#include "status.hpp"

#include <stdint.h>

namespace av {

class NtStatus {
public:
    static constexpr uint32_t monitor_no_more_descriptor_data = 0xC01D0003u;
    static constexpr uint32_t buffer_too_small = 0xC0000023u;

    explicit NtStatus(Status status);
    explicit NtStatus(uint32_t value);

    int32_t value() const;
    bool    succeeded() const;

private:
    static constexpr uint32_t internal_error = 0xC00000E5u;

    uint32_t value_;
};

}  // namespace av

#endif  // AV_NT_STATUS_HPP
