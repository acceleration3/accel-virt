/* The memory a host process's trace table lives in: /dev/shm/rvk-trace-<module>-<pid>,
 * which a reader maps directly and which is deliberately left behind when the
 * process ends, so a crashed QEMU's table can still be read (docs/trace.md).
 * Opening one first removes what processes no longer alive left there. */
#ifndef AV_SHM_TRACE_REGION_HPP
#define AV_SHM_TRACE_REGION_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ShmTraceRegion {
public:
    static constexpr uint32_t name_bytes = 64u;

    constexpr ShmTraceRegion() = default;
    ~ShmTraceRegion();
    ShmTraceRegion(const ShmTraceRegion &) = delete;
    ShmTraceRegion &operator=(const ShmTraceRegion &) = delete;

    /* `bytes` of zeroed shared memory named for `module` and this process.
     * invalid_state when open; the system's refusal as no_memory otherwise. */
    [[nodiscard]] Status open(const char *module, size_t bytes);
    /* The same under /dev/shm/<prefix><module>-<pid>: rvk-events- is the
     * timeline tools/perfetto_trace.py reads. */
    [[nodiscard]] Status open(const char *prefix, const char *module, size_t bytes);
    uint8_t             *region() const;
    size_t               bytes() const;
    uint32_t             process() const;

private:
    /* Files of dead processes: rvk-trace-*-<pid>, rvk-state-*-<pid>.sock and
     * crash-*-<pid>.txt stay (a crash file is what is left to read). */
    void sweep() const;

    uint8_t *region_ = nullptr;
    size_t   bytes_ = 0u;
    uint32_t process_ = 0u;
};

}  // namespace av

#endif  // AV_SHM_TRACE_REGION_HPP
