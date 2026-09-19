/* How large a recording may grow, and what happens when the next record would
 * pass that: the device's `record-limit` and `record-wrap` properties. */
#ifndef AV_RECORDING_LIMIT_HPP
#define AV_RECORDING_LIMIT_HPP

#include "device/recording/recording_header.hpp"

#include <stdint.h>

namespace av {

/* What a recording does once it is full. */
enum class AtCeiling : uint8_t { stop, wrap };

/* What the next record may do. */
enum class LimitVerdict : uint8_t { fits, stop, wrap };

class RecordingLimit {
public:
    static constexpr uint64_t default_ceiling = uint64_t{256} << 20;

    RecordingLimit();
    /* A ceiling of 0 is no ceiling; `written` starts at nothing. */
    RecordingLimit(uint64_t ceiling, AtCeiling at_ceiling);

    /* A record of `bytes` fits, forces a wrap, or stops the recording - and
     * stops it even when wrapping, for a record larger than the whole ceiling,
     * because a wrap would leave nowhere to put it either. */
    LimitVerdict verdict(uint64_t bytes) const;
    /* `bytes` were written. */
    void         took(uint64_t bytes);
    /* The file went back to holding nothing but its header. */
    void         wrapped();
    uint64_t     written() const;
    uint64_t     ceiling() const;

    bool invariants_hold() const;

private:
    uint64_t  ceiling_;
    uint64_t  written_;
    AtCeiling at_ceiling_;
};

}  // namespace av

#endif  // AV_RECORDING_LIMIT_HPP
