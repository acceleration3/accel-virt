/* A copy of a trace table, read by something that did not write it - the
 * guest's inspector reading another process's section or the driver's escape,
 * a crash handler reading its own - and therefore validated field by field
 * against the bytes held before any slot is looked at (docs/trace.md). */
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

    /* malformed unless `bytes` holds a whole version 1 table: the magic, the
     * layout's sizes, a slot count at most TraceLayout::max_slots whose slots
     * fit, and a NUL within the module name. Nothing is kept on failure. */
    [[nodiscard]] Status read(const uint8_t *bytes, size_t size);

    uint32_t      slots() const;
    TraceSlotView slot(uint32_t index) const;
    /* The module name, always terminated; empty before a read. */
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

}  // namespace av

#endif  // AV_TRACE_SNAPSHOT_HPP
