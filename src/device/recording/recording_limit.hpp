#ifndef AV_RECORDING_LIMIT_HPP
#define AV_RECORDING_LIMIT_HPP

#include "device/recording/recording_header.hpp"

#include <stdint.h>

namespace av {
enum class AtCeiling : uint8_t { stop, wrap };

enum class LimitVerdict : uint8_t { fits, stop, wrap };

class RecordingLimit {
public:
    static constexpr uint64_t default_ceiling = uint64_t{256} << 20;

    RecordingLimit();
    RecordingLimit(uint64_t ceiling, AtCeiling at_ceiling);

    LimitVerdict verdict(uint64_t bytes) const;
    void         took(uint64_t bytes);
    void         wrapped();
    uint64_t     written() const;
    uint64_t     ceiling() const;

    bool invariants_hold() const;

private:
    uint64_t  ceiling_;
    uint64_t  written_;
    AtCeiling at_ceiling_;
};

}

#endif
