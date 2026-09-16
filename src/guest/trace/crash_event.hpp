/* What a process knows when it crashes, before anything is formatted: which
 * binary's handler caught it, the process, the exception and where it
 * happened. */
#ifndef AV_CRASH_EVENT_HPP
#define AV_CRASH_EVENT_HPP

#include <stdint.h>

namespace av {

class CrashEvent {
public:
    CrashEvent(const char *module, uint32_t pid, uint32_t code, uint64_t address);

    const char *module() const;
    uint32_t    pid() const;
    uint32_t    code() const;
    uint64_t    address() const;

private:
    const char *module_;
    uint32_t    pid_;
    uint32_t    code_;
    uint64_t    address_;
};

}  // namespace av

#endif  // AV_CRASH_EVENT_HPP
