/* The 32 bytes in front of every record of a wire recording (docs/protocol.md):
 * what the record is, whose context and batch it belongs to, when the device
 * took it, and how many payload bytes follow. */
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
    /* A ring's largest data area and a reply slot's largest answer are both
     * 16 MiB, so no record the wire produces is larger. */
    static constexpr uint32_t max_payload = 1u << 24;
    static constexpr uint16_t lowest_kind = 1u;
    static constexpr uint16_t highest_kind = 7u;

    RecordHeader();
    RecordHeader(RecordKind kind, uint32_t context, uint32_t length, uint64_t timestamp, uint64_t fence);

    RecordKind kind() const;
    uint32_t   context() const;
    uint32_t   length() const;
    /* Nanoseconds since the recording's own start. */
    uint64_t   timestamp() const;
    /* The batch this record belongs to, 0 when it belongs to none. */
    uint64_t   fence() const;
    /* Bytes the whole record occupies: this header and its payload. */
    uint64_t   span() const;
    bool       fits(uint64_t remaining) const;

    /* invalid_parameter for a payload no recording may hold. */
    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

private:
    static constexpr uint32_t magic = 0x00445641u; /* "AVD" */

    RecordKind kind_;
    uint32_t   context_;
    uint32_t   length_;
    uint64_t   timestamp_;
    uint64_t   fence_;
};

}  // namespace av

#endif  // AV_RECORD_HEADER_HPP
