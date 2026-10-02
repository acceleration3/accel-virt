/* A state source that writes its reporter's dump on the socket's own thread:
 * the executor library's root, which takes no lock to report. */
#ifndef AV_REPORTER_STATE_SOURCE_HPP
#define AV_REPORTER_STATE_SOURCE_HPP

#include "device/shim/state_source.hpp"

#include <stddef.h>

namespace av {

class StateReporter;

class ReporterStateSource final : public StateSource {
public:
    explicit ReporterStateSource(StateReporter &reporter);

    size_t dump(char *buffer, size_t capacity, const char *&error) override;

private:
    StateReporter &reporter_;
};

}  // namespace av

#endif  // AV_REPORTER_STATE_SOURCE_HPP
