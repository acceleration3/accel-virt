/* InspectHost over Windows: Toolhelp's process list, a process's image path,
 * named sections and events opened by name, a file, and the interrupt time.
 * Translation only; every choice of name is InspectProgram's. */
#ifndef AV_UM_WINDOWS_INSPECT_HOST_HPP
#define AV_UM_WINDOWS_INSPECT_HOST_HPP

#include "guest/inspect/inspect_host.hpp"
#include "um/trace/interrupt_clock.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WindowsInspectHost final : public InspectHost {
public:
    WindowsInspectHost();

    uint32_t processes(uint32_t *pids, uint32_t capacity) override;
    Status   executable(uint32_t pid, char *path, size_t capacity) override;
    Status   copy_section(const char *name, uint8_t *destination, size_t capacity, size_t &copied) override;
    Status   request_state(const char *dump, const char *done, uint32_t milliseconds) override;
    Status   write_file(const char *path, const uint8_t *head, size_t head_bytes, const uint8_t *body,
                        size_t body_bytes) override;
    uint64_t clock() const override;

private:
    InterruptClock clock_;
};

}  // namespace av

#endif  // AV_UM_WINDOWS_INSPECT_HOST_HPP
