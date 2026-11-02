#ifndef AV_RECORD_HEADER_HPP
#define AV_RECORD_HEADER_HPP

#include "core/support/status.hpp"
#include "device/recording/record_kind.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

class RecordHeader {
public:
    static constexpr size_t   encoded_size = 32u;
    static constexpr uint32_t max_payload = 1u << 24;
    static constexpr uint16_t lowest_kind = 1u;
    static constexpr uint16_t highest_kind = 7u;

    RecordHeader();
    RecordHeader(RecordKind kind, uint32_t context, uint32_t length, uint64_t timestamp, uint64_t fence);

    RecordKind kind() const;
    uint32_t   context() const;
    uint32_t   length() const;
    uint64_t   timestamp() const;
    uint64_t   fence() const;
    uint64_t   span() const;
    bool       fits(uint64_t remaining) const;

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00445641u;

    RecordKind kind_;
    uint32_t   context_;
    uint32_t   length_;
    uint64_t   timestamp_;
    uint64_t   fence_;
};

}

#endif
