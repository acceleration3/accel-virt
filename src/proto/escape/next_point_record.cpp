#include "proto/escape/next_point_record.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

NextPointRecord::NextPointRecord() : surface_(0u), point_(0u)
{
    AV_SCOPE("NextPointRecord::NextPointRecord");
}

NextPointRecord::NextPointRecord(uint64_t surface, uint32_t point) : surface_(surface), point_(point)
{
    AV_SCOPE("NextPointRecord::NextPointRecord");
}

uint64_t NextPointRecord::surface() const
{
    AV_SCOPE("NextPointRecord::surface");
    return surface_;
}

uint32_t NextPointRecord::point() const
{
    AV_SCOPE("NextPointRecord::point");
    return point_;
}

Status NextPointRecord::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("NextPointRecord::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(point_);
    writer.write_u64(surface_);
    return writer.status();
}

Status NextPointRecord::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("NextPointRecord::decode");
    if (length != encoded_size) return Status::malformed;
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t point = reader.read_u32();
    const uint64_t surface = reader.read_u64();
    if (reader.status() != Status::ok) return Status::malformed;
    if (found_magic != magic || point == 0u || surface == 0u) return Status::malformed;
    surface_ = surface;
    point_ = point;
    return Status::ok;
}

}  // namespace av
