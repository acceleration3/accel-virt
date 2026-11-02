#ifndef AV_PRESENT_POINT_RECORD_HPP
#define AV_PRESENT_POINT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class PresentPointRecord {
public:
    static constexpr size_t encoded_size = 16u;

    PresentPointRecord();
    explicit PresentPointRecord(uint32_t point);

    uint32_t point() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00505641u;

    uint32_t point_;
};

}

#endif
