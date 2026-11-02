#ifndef AV_WAKE_EVENT_RECORD_HPP
#define AV_WAKE_EVENT_RECORD_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class WakeEventRecord {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t magic        = 0x00455641u;

    WakeEventRecord();
    explicit WakeEventRecord(uint64_t event);

    uint64_t event() const;
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint16_t version = 1u;

    uint64_t event_;
};

}

#endif
