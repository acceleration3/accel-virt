/* The driver's trace table (docs/trace.md): laid out over contiguous memory at
 * DriverEntry, before the miniport exists, so every DDI's scopes land in it;
 * bound for AV_SCOPE; its physical address logged for a host to read from a
 * wedged guest; saved in the crash dump and written to the debug port when the
 * system bugchecks. Tracing that cannot start is logged once and left off -
 * the driver loads either way. One per driver image. */
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

    /* `image` is the driver's own address range, which the names its frames
     * point at lie in. */
    DriverTrace(Platform &platform, TraceHost &host, const ImageRange &image);
    DriverTrace(const DriverTrace &) = delete;
    DriverTrace &operator=(const DriverTrace &) = delete;

    /* PASSIVE. Once; a later call changes nothing. A null `routine` registers
     * nothing to run at a bugcheck. */
    void start(BugcheckRoutine routine);
    /* PASSIVE, with no thread left inside the driver: unbinds, deregisters and
     * releases, in that order. */
    void stop();
    /* From the bugcheck routine: every line of the table to the debug port. */
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

}  // namespace av

#endif  // AV_DRIVER_TRACE_HPP
