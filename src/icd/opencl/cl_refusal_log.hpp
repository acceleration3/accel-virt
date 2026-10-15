/* The calls an application was refused: an entry point this ICD does not
 * serve, or one that answered an error. Each entry point, with each error it
 * answered, is named in the log the first time. An application that only
 * reports "a problem" - Photoshop's Blur Gallery - otherwise leaves nothing to
 * say which call it was refused, and one that retries in a loop would
 * otherwise write a line per call. */
#ifndef AV_CLICD_CL_REFUSAL_LOG_HPP
#define AV_CLICD_CL_REFUSAL_LOG_HPP

#include "core/log/keyed_log.hpp"

#include <stdint.h>

namespace av {
class LogSink;
}  // namespace av

namespace av::clicd {

class ClRefusalLog {
public:
    /* Room for every slot of cl_icd_dispatch this ICD leaves to the generated
     * refusals and a few errors from each entry point it serves. */
    static constexpr uint32_t max_named = KeyedLog::max_keys;

    explicit ClRefusalLog(LogSink &log);
    ClRefusalLog(const ClRefusalLog &) = delete;
    ClRefusalLog &operator=(const ClRefusalLog &) = delete;

    /* `entry_point` is a literal, the generated refusal's or the hook's, so
     * one entry point is one pointer. */
    void     note(const char *entry_point);
    void     note_failure(const char *entry_point, int32_t error);
    /* A query `entry_point` refused, named with the query the first time it
     * is: what says which of an application's questions this device cannot
     * answer, which the entry point alone does not. */
    void     note_query(const char *entry_point, uint32_t query, int32_t error);
    /* A map refused for `why` (a literal, logged the first time): its size
     * and how many of the context's mappings were held, which is what says
     * whether a limit or the memory ran out. */
    void     note_map(const char *why, uint64_t bytes, uint32_t held);
    /* A buffer released with `mappings` of it never unmapped, logged the first
     * time: what an application that runs out of mappings may be doing. */
    void     note_released_mapped(uint32_t mappings);
    /* A call that returns before its work is done ran ahead of a user event
     * the application had not set yet (ClIcdRoot::without_unset_user_events),
     * logged the first time: the one place a wait list is not honoured. */
    void     note_ran_ahead_of_user_event();
    uint32_t named() const;

private:
    KeyedLog  lines_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_REFUSAL_LOG_HPP
