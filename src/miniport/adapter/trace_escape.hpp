#ifndef AV_TRACE_ESCAPE_HPP
#define AV_TRACE_ESCAPE_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class Platform;
class StateReporter;
class TraceEscapeRecord;

class TraceEscape {
public:
    static constexpr size_t state_bytes = 256u * 1024u;

    TraceEscape(Platform &platform, StateReporter &reporter);
    TraceEscape(const TraceEscape &) = delete;
    TraceEscape &operator=(const TraceEscape &) = delete;

    [[nodiscard]] Status answer(uint8_t *bytes, size_t length);

private:
    void reply(uint8_t *bytes, size_t length, const TraceEscapeRecord &request, const uint8_t *source,
               uint64_t total);

    Platform      &platform_;
    StateReporter &reporter_;
};

}

#endif
