/* One host API, as a session sees it.
 *
 * Virtual because there are three real implementations and they have nothing
 * in common below this line: Vulkan and OpenCL each derive from a generated
 * abstract dispatch of their own, and OpenGL has no dispatch object at all -
 * its generated decoder takes a table of function pointers. A session must not
 * know which of those it is holding, so this is where the three become one
 * thing (feature 042).
 *
 * Everything here runs on the context's worker, one call at a time. */
#ifndef AV_SESSION_BACKEND_HPP
#define AV_SESSION_BACKEND_HPP

#include <stdint.h>

namespace av {

class WireArena;
class WireReader;
class WireWriter;

class SessionBackend {
public:
    SessionBackend(const SessionBackend &) = delete;
    SessionBackend &operator=(const SessionBackend &) = delete;

    /* Whether this backend decodes `command_id` at all. An id the profile did
     * not select is skipped by the device, not faulted, so the two answers must
     * not be confused. */
    virtual bool knows(uint32_t command_id) const = 0;
    /* Decodes one command and runs it, writing whatever answer it has into
     * `reply`. False for a payload that does not decode - including one the
     * arena would not give room for, which is the caller's to tell apart. */
    [[nodiscard]] virtual bool run(uint32_t command_id, WireReader &payload, WireWriter &reply,
                                   WireArena &arena) = 0;
    /* Whether the host API has lost the device this backend runs on. Asked
     * after every command, because nothing else would notice. */
    virtual bool lost() const = 0;

protected:
    SessionBackend() = default;
    ~SessionBackend() = default;
};

}  // namespace av

#endif  // AV_SESSION_BACKEND_HPP
