#ifndef AV_FOLLOW_RECORD_HPP
#define AV_FOLLOW_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class FollowRecord {
public:
    static constexpr size_t encoded_size = 16u;

    FollowRecord();
    FollowRecord(uint32_t ring, uint64_t fence);

    uint32_t ring() const;
    uint64_t fence() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00465641u;

    uint32_t ring_;
    uint64_t fence_;
};

}

#endif
