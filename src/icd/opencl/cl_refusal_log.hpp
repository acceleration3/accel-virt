/* What OpenCL alone refuses an application, beyond the calls every driver
 * names through icd::RefusedCalls: a query with the question it was asked,
 * a map with what ran out, a buffer released while mapped, and a transfer run
 * ahead of a user event. Each is named in the ICD's log the first time. An
 * application that only reports "a problem" - Photoshop's Blur Gallery -
 * otherwise leaves nothing to say which call it was refused. */
#ifndef AV_CLICD_CL_REFUSAL_LOG_HPP
#define AV_CLICD_CL_REFUSAL_LOG_HPP

#include "core/log/keyed_log.hpp"

#include <stdint.h>

namespace av::clicd {

class ClRefusalLog {
public:
    explicit ClRefusalLog(KeyedLog &lines);
    ClRefusalLog(const ClRefusalLog &) = delete;
    ClRefusalLog &operator=(const ClRefusalLog &) = delete;

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
    KeyedLog &lines_;
};

}  // namespace av::clicd

#endif  // AV_CLICD_CL_REFUSAL_LOG_HPP
