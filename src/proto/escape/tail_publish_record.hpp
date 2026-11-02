#ifndef AV_TAIL_PUBLISH_RECORD_HPP
#define AV_TAIL_PUBLISH_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class TailPublishRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x50564B52u;

    TailPublishRecord();
    explicit TailPublishRecord(uint32_t ring);

    uint32_t ring() const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    uint32_t ring_;
};

}

#endif
