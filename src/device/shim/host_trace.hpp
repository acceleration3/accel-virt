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
    ShmTraceRegion events_region_;
    EventRing      events_;
    ImageRange     image_;
};

}

#endif
