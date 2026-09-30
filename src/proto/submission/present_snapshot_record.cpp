#include "proto/submission/present_snapshot_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

PresentSnapshotRecord::PresentSnapshotRecord() : key_(0u)
{
    AV_SCOPE("PresentSnapshotRecord::PresentSnapshotRecord");
}

PresentSnapshotRecord::PresentSnapshotRecord(uint64_t key) : key_(key)
{
    AV_SCOPE("PresentSnapshotRecord::PresentSnapshotRecord");
}

uint64_t PresentSnapshotRecord::key() const
{
    AV_SCOPE("PresentSnapshotRecord::key");
    return key_;
}

Status PresentSnapshotRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("PresentSnapshotRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(0u);
    writer.write_u64(key_);
    return writer.status();
}

Status PresentSnapshotRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("PresentSnapshotRecord::decode");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t reserved = reader.read_u32();
    const uint64_t key = reader.read_u64();
    if (reader.status() != Status::ok) return Status::malformed;
    if (found_magic != magic || reserved != 0u || key == 0u) return Status::malformed;
    key_ = key;
    return Status::ok;
}

}  // namespace av
