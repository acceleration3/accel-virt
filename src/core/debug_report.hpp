/* The adapter's state when dxgkrnl collects debug information for a TDR
 * report: a fixed little-endian layout a person can find in a dump by its
 * magic. Writing allocates nothing and takes no lock, since collection runs
 * while the adapter is being recovered. */
#ifndef AV_DEBUG_REPORT_HPP
#define AV_DEBUG_REPORT_HPP

#include "platform.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class DebugReport {
public:
    static constexpr uint32_t magic = 0x444B5652u;   /* "RVKD" */
    static constexpr uint16_t format = 1u;
    /* magic, format, started, powered, negotiated features */
    static constexpr size_t   report_bytes = 16u;

    DebugReport(Platform &platform, bool started, bool powered, uint64_t features);

    /* The whole fields that fit in `capacity`; answers the bytes written. */
    size_t write(uint8_t *buffer, size_t capacity) const;

private:
    Platform &platform_;
    bool      started_;
    bool      powered_;
    uint64_t  features_;
};

}  // namespace av

#endif  // AV_DEBUG_REPORT_HPP
