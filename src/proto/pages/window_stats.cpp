#include "proto/pages/window_stats.hpp"

#include "core/trace/function_scope.hpp"
#include "proto/wire/wire_reader.hpp"
#include "proto/wire/wire_writer.hpp"

namespace av {

WindowStats::WindowStats() : fields_{}
{
    AV_SCOPE("WindowStats::WindowStats");
}

WindowStats::WindowStats(uint64_t total_pages, uint64_t pages_in_use, uint64_t peak_pages_in_use, uint64_t refusals,
                         uint64_t refused_bytes, uint64_t largest_free_run)
    : fields_{total_pages, pages_in_use, peak_pages_in_use, refusals, refused_bytes, largest_free_run}
{
    AV_SCOPE("WindowStats::WindowStats");
}

uint64_t WindowStats::total_pages() const
{
    AV_SCOPE("WindowStats::total_pages");
    return fields_[0];
}

uint64_t WindowStats::pages_in_use() const
{
    AV_SCOPE("WindowStats::pages_in_use");
    return fields_[1];
}

uint64_t WindowStats::peak_pages_in_use() const
{
    AV_SCOPE("WindowStats::peak_pages_in_use");
    return fields_[2];
}

uint64_t WindowStats::refusals() const
{
    AV_SCOPE("WindowStats::refusals");
    return fields_[3];
}

uint64_t WindowStats::refused_bytes() const
{
    AV_SCOPE("WindowStats::refused_bytes");
    return fields_[4];
}

uint64_t WindowStats::largest_free_run() const
{
    AV_SCOPE("WindowStats::largest_free_run");
    return fields_[5];
}

uint64_t WindowStats::field(uint32_t index) const
{
    AV_SCOPE("WindowStats::field");
    return index < field_count ? fields_[index] : 0u;
}

Status WindowStats::encode(uint8_t *bytes, size_t capacity) const
{
    AV_SCOPE("WindowStats::encode");
    if (capacity < encoded_size) return Status::too_large;
    WireWriter writer(bytes, capacity);
    writer.write_u32(magic);
    writer.write_u32(version);
    writer.write_u32(0u);
    writer.write_u32(0u);
    for (const uint64_t value : fields_) writer.write_u64(value);
    return writer.status();
}

Status WindowStats::decode(const uint8_t *bytes, size_t length)
{
    AV_SCOPE("WindowStats::decode");
    WireReader     reader(bytes, length);
    const uint32_t found_magic = reader.read_u32();
    const uint32_t found_version = reader.read_u32();
    reader.skip(fields_at - sequence_at);
    uint64_t values[field_count] = {};
    for (uint64_t &value : values) value = reader.read_u64();
    if (reader.status() != Status::ok) return reader.status();
    if (found_magic != magic || found_version != version) return Status::malformed;
    for (uint32_t index = 0u; index < field_count; index++) fields_[index] = values[index];
    return Status::ok;
}

}
