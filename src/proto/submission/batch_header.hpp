#ifndef AV_BATCH_HEADER_HPP
#define AV_BATCH_HEADER_HPP

#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class BatchHeader {
public:
    static constexpr size_t encoded_size = 32u;

    BatchHeader();
    BatchHeader(uint32_t length, uint64_t fence_id);
    BatchHeader(uint32_t length, uint64_t fence_id, uint64_t published_at);
    uint64_t published_at() const;

    uint32_t length() const;
    uint64_t fence_id() const;
    uint64_t span() const;
    bool     fits(uint64_t capacity) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic     = 0x00425641u;
    static constexpr uint64_t alignment = 8u;

    uint32_t length_;
    uint64_t fence_id_;
    uint64_t published_at_;
};

}

#endif
