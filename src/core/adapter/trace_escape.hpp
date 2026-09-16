/* The driver's answer to rvkinspect.exe's escape (docs/trace.md): one window
 * of the bound trace table, or of this adapter's state dump built for the
 * call. The request is judged against the bytes it arrived in and the answer
 * is written into the same bytes; nothing is written past them. PASSIVE. */
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
    /* A dump is built in this much non-paged memory: it is written while the
     * reporter holds spin locks. */
    static constexpr size_t state_bytes = 256u * 1024u;

    TraceEscape(Platform &platform, StateReporter &reporter);
    TraceEscape(const TraceEscape &) = delete;
    TraceEscape &operator=(const TraceEscape &) = delete;

    /* ok with the answer written; malformed for a request that is not one,
     * with nothing written; no_memory when the dump has nowhere to go. */
    [[nodiscard]] Status answer(uint8_t *bytes, size_t length);

private:
    /* The window of `total` bytes at `source` the request names. */
    void reply(uint8_t *bytes, size_t length, const TraceEscapeRecord &request, const uint8_t *source,
               uint64_t total);

    Platform      &platform_;
    StateReporter &reporter_;
};

}  // namespace av

#endif  // AV_TRACE_ESCAPE_HPP
