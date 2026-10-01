/* The first 64 bytes of a wire recording (docs/protocol.md): what wrote it,
 * which wire version it recorded, which record kinds it keeps, the ceiling it
 * is bounded by, how often it has wrapped back to here, and the host GPU the
 * recorded device was bound to. */
#ifndef AV_RECORDING_HEADER_HPP
#define AV_RECORDING_HEADER_HPP

#include "core/support/status.hpp"
#include "proto/context/protocol_version.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct RecordingHeaderProbe;

class RecordingHeader {
public:
    static constexpr size_t   encoded_size = 64u;
    static constexpr size_t   uuid_bytes = 16u;
    static constexpr uint16_t current_major = 1u;
    static constexpr uint16_t current_minor = 0u;

    RecordingHeader();
    /* `uuid` is uuid_bytes of the bound GPU's Vulkan device UUID; null is
     * sixteen zeroes, which is what a device bound to no GPU recorded. */
    RecordingHeader(const ProtocolVersion &wire, uint32_t kinds, uint64_t started, uint64_t limit,
                    uint32_t vendor_id, uint32_t device_id, const uint8_t *uuid);

    uint16_t        major_version() const;
    uint16_t        minor_version() const;
    ProtocolVersion wire_version() const;
    uint32_t        kinds() const;
    uint64_t        started() const;
    /* The ceiling in bytes, 0 for none. */
    uint64_t        limit() const;
    uint32_t        wraps() const;
    uint32_t        vendor_id() const;
    uint32_t        device_id() const;
    const uint8_t  *uuid() const;

    /* The recording wrapped back to this header; the count saturates rather
     * than telling a reader it wrapped less often than it did. */
    void wrapped();

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    /* A major this reader does not know is not_supported; anything else the
     * bytes cannot prove is malformed. A minor above this reader's is read as
     * far as this version goes. Unchanged unless ok. */
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

    bool invariants_hold() const;

private:
    static constexpr uint32_t magic = 0x00525641u; /* "AVR" */

    uint16_t major_;
    uint16_t minor_;
    uint16_t wire_major_;
    uint16_t wire_minor_;
    uint32_t kinds_;
    uint64_t started_;
    uint64_t limit_;
    uint32_t wraps_;
    uint32_t vendor_id_;
    uint32_t device_id_;
    uint8_t  uuid_[uuid_bytes];

    friend struct RecordingHeaderProbe;
};

}  // namespace av

#endif  // AV_RECORDING_HEADER_HPP
