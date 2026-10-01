/* One host binary's trace table: laid out over shared memory named for the
 * binary and this process, bound for AV_SCOPE, with the binary's own address
 * range for naming its frames. The device module and the executor library
 * each hold one (docs/trace.md). Tracing that cannot start stays off. */
#ifndef AV_HOST_TRACE_HPP
#define AV_HOST_TRACE_HPP

#include "core/support/status.hpp"
#include "core/trace/event_ring.hpp"
#include "core/trace/image_range.hpp"
#include "core/trace/trace_table.hpp"
#include "device/shim/posix_trace_host.hpp"
#include "device/shim/shm_trace_region.hpp"

#include <stdint.h>

namespace av {

class HostTrace {
public:
    static constexpr uint32_t slots = 512u;

    HostTrace();
    HostTrace(const HostTrace &) = delete;
    HostTrace &operator=(const HostTrace &) = delete;

    /* Once: invalid_state after. The region's refusal otherwise. */
    [[nodiscard]] Status start(const char *module);
    bool                 started() const;
    TraceTable          &table();
    const ImageRange    &image() const;
    const TraceHost     &host() const;
    const uint8_t       *region() const;
    size_t               bytes() const;
    uint32_t             process() const;

private:
    PosixTraceHost host_;
    ShmTraceRegion region_;
    TraceTable     table_;
    /* AV_PERFETTO=1 in the environment: every scope is also an event. */
    ShmTraceRegion events_region_;
    EventRing      events_;
    ImageRange     image_;
};

}  // namespace av

#endif  // AV_HOST_TRACE_HPP
