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

    void     note_query(const char *entry_point, uint32_t query, int32_t error);
    void     note_map(const char *why, uint64_t bytes, uint32_t held);
    void     note_released_mapped(uint32_t mappings);
    void     note_ran_ahead_of_user_event();
    uint32_t named() const;

private:
    KeyedLog &lines_;
};

}

#endif
