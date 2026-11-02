#ifndef AV_TRACE_SNAPSHOT_HPP
#define AV_TRACE_SNAPSHOT_HPP

#include "core/support/status.hpp"
#include "core/trace/trace_layout.hpp"
#include "core/trace/trace_slot_view.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct TraceSnapshotProbe;

class TraceSnapshot {
public:
    TraceSnapshot();

    [[nodiscard]] Status read(const uint8_t *bytes, size_t size);

    uint32_t      slots() const;
    TraceSlotView slot(uint32_t index) const;
    const char   *module() const;
    uint32_t      process() const;
    uint64_t      created() const;
    uint32_t      crash_code() const;
    uint64_t      crash_address() const;
    uint32_t      crash_thread() const;
    uint64_t      crashed_at() const;

    bool invariants_hold() const;

private:
    uint64_t field(uint32_t offset, uint32_t bytes) const;

    const uint8_t *bytes_;
    TraceLayout    layout_;
    char           module_[TraceLayout::module_bytes];

    friend struct TraceSnapshotProbe;
};

}

#endif
