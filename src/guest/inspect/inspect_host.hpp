/* What rvkinspect.exe needs of Windows (docs/trace.md): the processes, a named
 * section's bytes, a process's state service, a file and the clock. Names are
 * ASCII object names (`Global\rvk-trace-rvkvk-1234`); every decision about
 * which name to try is InspectProgram's. WindowsInspectHost (src/um) and
 * MockInspectHost (tests) implement it. */
#ifndef AV_INSPECT_HOST_HPP
#define AV_INSPECT_HOST_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class InspectHost {
public:
    InspectHost() = default;
    InspectHost(const InspectHost &) = delete;
    InspectHost &operator=(const InspectHost &) = delete;

    /* How many process ids were written, at most `capacity`. */
    virtual uint32_t processes(uint32_t *pids, uint32_t capacity) = 0;
    /* The executable's path, cut to fit and terminated; not_found for a
     * process that cannot be opened. */
    virtual Status   executable(uint32_t pid, char *path, size_t capacity) = 0;
    /* Copies the first bytes of the section `name`, at most `capacity`;
     * not_found when no such section can be opened. */
    virtual Status   copy_section(const char *name, uint8_t *destination, size_t capacity, size_t &copied) = 0;
    /* Signals the event `dump` and waits `milliseconds` for the event `done`:
     * not_found when either cannot be opened, invalid_state when `done` was not
     * signalled in time. */
    virtual Status   request_state(const char *dump, const char *done, uint32_t milliseconds) = 0;
    /* Creates or replaces the file at `path` with `head` then `body`. */
    virtual Status   write_file(const char *path, const uint8_t *head, size_t head_bytes, const uint8_t *body,
                                size_t body_bytes) = 0;
    /* The interrupt time trace tables stamp, in 100 ns. */
    virtual uint64_t clock() const = 0;

protected:
    ~InspectHost() = default;
};

}  // namespace av

#endif  // AV_INSPECT_HOST_HPP
