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

}

#endif
