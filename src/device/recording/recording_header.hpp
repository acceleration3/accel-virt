#ifndef AV_RECORDING_HEADER_HPP
#define AV_RECORDING_HEADER_HPP

#include "core/support/status.hpp"
#include "proto/context/device_uuid.hpp"
#include "proto/context/protocol_version.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {

struct RecordingHeaderProbe;

class RecordingHeader {
public:
    static constexpr size_t   encoded_size = 64u;
    static constexpr size_t   uuid_bytes = DeviceUuid::bytes;
    static constexpr uint16_t current_major = 1u;
    static constexpr uint16_t current_minor = 0u;

    RecordingHeader();
    RecordingHeader(const ProtocolVersion &wire, uint32_t kinds, uint64_t started, uint64_t limit,
                    uint32_t vendor_id, uint32_t device_id, const uint8_t *uuid);

    uint16_t        major_version() const;
    uint16_t        minor_version() const;
    ProtocolVersion wire_version() const;
    uint32_t        kinds() const;
    uint64_t        started() const;
    uint64_t        limit() const;
    uint32_t        wraps() const;
    uint32_t        vendor_id() const;
    uint32_t        device_id() const;
    const uint8_t  *uuid() const;

    void wrapped();

    [[nodiscard]] Status encode(uint8_t *bytes, size_t capacity) const;
    [[nodiscard]] Status decode(const uint8_t *bytes, size_t length);

    bool invariants_hold() const;

private:
    static constexpr uint32_t magic = 0x00525641u;

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
    DeviceUuid uuid_;

    friend struct RecordingHeaderProbe;
};

}

#endif
