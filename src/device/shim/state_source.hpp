/* What a state socket answers `state\n` with (docs/trace.md): the device's
 * dump, which its main loop writes under QEMU's lock, or the executor's, which
 * its root writes where it is asked. Two implementations: DeviceTrace and
 * ReporterStateSource. */
#ifndef AV_STATE_SOURCE_HPP
#define AV_STATE_SOURCE_HPP

#include <stddef.h>

namespace av {

class StateSource {
public:
    StateSource() = default;
    StateSource(const StateSource &) = delete;
    StateSource &operator=(const StateSource &) = delete;

    /* The dump written into `buffer` and its length; or 0, with `error` set to
     * the whole line - ending in a newline - that says why there is none. */
    virtual size_t dump(char *buffer, size_t capacity, const char *&error) = 0;

protected:
    ~StateSource() = default;
};

}  // namespace av

#endif  // AV_STATE_SOURCE_HPP
