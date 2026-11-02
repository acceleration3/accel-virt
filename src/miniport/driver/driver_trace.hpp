#ifndef AV_DRIVER_TRACE_HPP
#define AV_DRIVER_TRACE_HPP

#include "core/platform/platform.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/trace_table.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class TraceHost;
struct DriverTraceProbe;

class DriverTrace {
public:
    static constexpr uint32_t slots = 64u;

    DriverTrace(Platform &platform, TraceHost &host, const ImageRange &image);
    DriverTrace(const DriverTrace &) = delete;
    DriverTrace &operator=(const DriverTrace &) = delete;

    void start(BugcheckRoutine routine);
    void stop();
    void bugcheck();

    bool invariants_hold() const;

private:
    static constexpr size_t line_bytes = 256u;

    Platform   &platform_;
    TraceHost  &host_;
    TraceTable  table_;
    ImageRange  image_;
    uint8_t    *region_;
    size_t      bytes_;
    bool        registered_;

    friend struct DriverTraceProbe;
};

}

#endif
