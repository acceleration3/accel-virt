/* A process's state dump on request (docs/trace.md): a thread waits on
 * `av-dump-<module>-<pid>`, writes the reporter's dump into the section
 * `av-state-<module>-<pid>` - a u32 length, then the text - and signals
 * `av-done-<module>-<pid>`. The thread is the one thing here that runs on its
 * own: whatever the process's other threads are stuck on, a dump is written
 * (the reporter never waits for a lock). Translation only. */
#ifndef AV_UM_STATE_SERVICE_HPP
#define AV_UM_STATE_SERVICE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class ObjectSecurity;
class StateReporter;

class StateService {
public:
    static constexpr size_t section_bytes = 256u * 1024u;

    StateService();
    StateService(const StateService &) = delete;
    StateService &operator=(const StateService &) = delete;

    /* invalid_state when started already; no_memory when the objects or the
     * thread cannot be made, with nothing left made. */
    [[nodiscard]] Status start(const char *module, uint32_t pid, StateReporter &reporter, ObjectSecurity &security);
    /* The service thread's body; returns only if a wait fails. */
    void                 serve();

private:
    [[nodiscard]] bool make(const char *scope, const char *module, uint32_t pid, ObjectSecurity &security);
    void               close();

    StateReporter *reporter_;
    void          *dump_;
    void          *done_;
    void          *section_;
    uint8_t       *view_;
    void          *thread_;
};

}  // namespace av

#endif  // AV_UM_STATE_SERVICE_HPP
