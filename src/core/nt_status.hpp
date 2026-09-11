/* A core Status carried across the DDI boundary as the NTSTATUS dxgkrnl
 * expects, plus the few graphics statuses core has no name for. */
#ifndef AV_NT_STATUS_HPP
#define AV_NT_STATUS_HPP

#include "status.hpp"

#include <stdint.h>

namespace av {

class NtStatus {
public:
    /* What basicdisplay.sys answers DxgkDdiQueryDeviceDescriptor with no EDID. */
    static constexpr uint32_t child_descriptor_not_supported = 0xC01E0401u;
    static constexpr uint32_t buffer_too_small = 0xC0000023u;

    explicit NtStatus(Status status);
    explicit NtStatus(uint32_t value);

    int32_t value() const;
    bool    succeeded() const;
    /* A failure core has no name for is invalid_state: the peer refused. */
    Status  status() const;

private:
    static constexpr uint32_t internal_error = 0xC00000E5u;

    uint32_t value_;
};

}  // namespace av

#endif  // AV_NT_STATUS_HPP
