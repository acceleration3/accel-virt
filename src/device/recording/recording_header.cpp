#include "device/recording/recording_header.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

RecordingHeader::RecordingHeader()
    : major_(current_major),
      minor_(current_minor),
      wire_major_(ProtocolVersion::current_major),
      wire_minor_(ProtocolVersion::current_minor),
      kinds_(0u),
      started_(0u),
      limit_(0u),
      wraps_(0u),
      vendor_id_(0u),
      device_id_(0u),
      uuid_()
{
    AV_SCOPE("RecordingHeader::RecordingHeader");
}

RecordingHeader::RecordingHeader(const ProtocolVersion &wire, uint32_t kinds, uint64_t started,
                                 uint64_t limit, uint32_t vendor_id, uint32_t device_id,
                                 const uint8_t *uuid)
    : major_(current_major),
      minor_(current_minor),
      wire_major_(wire.major_version()),
      wire_minor_(wire.minor_version()),
      kinds_(kinds),
      started_(started),
      limit_(limit),
      wraps_(0u),
      vendor_id_(vendor_id),
      device_id_(device_id),
      uuid_()
{
    AV_SCOPE("RecordingHeader::RecordingHeader");
    if (uuid == nullptr) return;
    for (size_t index = 0u; index < uuid_bytes; index++) uuid_[index] = uuid[index];
}

uint16_t RecordingHeader::major_version() const
{
    AV_SCOPE("RecordingHeader::major_version");
    return major_;
}

uint16_t RecordingHeader::minor_version() const
{
    AV_SCOPE("RecordingHeader::minor_version");
    return minor_;
}

ProtocolVersion RecordingHeader::wire_version() const
{
    AV_SCOPE("RecordingHeader::wire_version");
    return ProtocolVersion(wire_major_, wire_minor_);
}

uint32_t RecordingHeader::kinds() const
{
    AV_SCOPE("RecordingHeader::kinds");
    return kinds_;
}

uint64_t RecordingHeader::started() const
{
    AV_SCOPE("RecordingHeader::started");
    return started_;
}

uint64_t RecordingHeader::limit() const
{
    AV_SCOPE("RecordingHeader::limit");
    return limit_;
}

uint32_t RecordingHeader::wraps() const
{
    AV_SCOPE("RecordingHeader::wraps");
    return wraps_;
}

uint32_t RecordingHeader::vendor_id() const
{
    AV_SCOPE("RecordingHeader::vendor_id");
    return vendor_id_;
}

uint32_t RecordingHeader::device_id() const
{
    AV_SCOPE("RecordingHeader::device_id");
    return device_id_;
}

const uint8_t *RecordingHeader::uuid() const
{
    AV_SCOPE("RecordingHeader::uuid");
    return uuid_;
}

void RecordingHeader::wrapped()
{
    AV_SCOPE("RecordingHeader::wrapped");
    if (wraps_ == 0xFFFFFFFFu) return;
    wraps_++;
}

Status RecordingHeader::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("RecordingHeader::encode");
    if (!invariants_hold()) return Status::invalid_parameter;
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u16(major_);
    writer.write_u16(minor_);
    writer.write_u16(wire_major_);
    writer.write_u16(wire_minor_);
    writer.write_u32(kinds_);
    writer.write_u64(started_);
    writer.write_u64(limit_);
    writer.write_u32(wraps_);
    writer.write_u32(vendor_id_);
    writer.write_u32(device_id_);
    writer.write_u32(0u);
    writer.copy_bytes(uuid_, uuid_bytes);
    return writer.status();
}

Status RecordingHeader::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("RecordingHeader::decode");
    WireReader reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint16_t major = reader.read_u16();
    const uint16_t minor = reader.read_u16();
    const uint16_t wire_major = reader.read_u16();
    const uint16_t wire_minor = reader.read_u16();
    const uint32_t kinds = reader.read_u32();
    const uint64_t started = reader.read_u64();
    const uint64_t limit = reader.read_u64();
    const uint32_t wraps = reader.read_u32();
    const uint32_t vendor_id = reader.read_u32();
    const uint32_t device_id = reader.read_u32();
    reader.expect_zero(4u);
    uint8_t uuid[uuid_bytes] = {};
    reader.copy_bytes(uuid, uuid_bytes);
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic) return Status::malformed;
    if (limit != 0u && limit < encoded_size) return Status::malformed;
    if (major != current_major) return Status::not_supported;
    RecordingHeader decoded(ProtocolVersion(wire_major, wire_minor), kinds, started, limit, vendor_id,
                            device_id, uuid);
    decoded.minor_ = minor;
    decoded.wraps_ = wraps;
    *this = decoded;
    return Status::ok;
}

bool RecordingHeader::invariants_hold() const
{
    AV_SCOPE("RecordingHeader::invariants_hold");
    if (major_ != current_major) return false;
    return limit_ == 0u || limit_ >= encoded_size;
}

}  // namespace av
