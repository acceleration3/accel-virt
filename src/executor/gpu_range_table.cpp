#include "executor/gpu_range_table.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av {

GpuRangeTable::GpuRangeTable(uint64_t fallback) : fallback_(fallback)
{
    AV_SCOPE("GpuRangeTable::GpuRangeTable");
}

uint64_t GpuRangeTable::bytes_for(uint32_t rows) const
{
    AV_SCOPE("GpuRangeTable::bytes_for");
    return header_bytes + uint64_t{rows} * sizeof(Row);
}

uint64_t GpuRangeTable::fallback() const
{
    AV_SCOPE("GpuRangeTable::fallback");
    return fallback_;
}

bool GpuRangeTable::fill(void *out, uint64_t capacity, const Row *rows, uint32_t count) const
{
    AV_SCOPE("GpuRangeTable::fill");
    if (out == nullptr || (count != 0u && rows == nullptr) || capacity < bytes_for(count)) return false;
    for (uint32_t index = 0u; index < count; index++) {
        if (rows[index].bytes_ == 0u || rows[index].guest_ + rows[index].bytes_ < rows[index].guest_) return false;
        if (index != 0u && rows[index].guest_ - rows[index - 1u].guest_ < rows[index - 1u].bytes_) return false;
        if (index != 0u && rows[index].guest_ < rows[index - 1u].guest_) return false;
    }
    uint8_t *const bytes = static_cast<uint8_t *>(out);
    const uint32_t header[2] = {count, 0u};
    memcpy(bytes, header, sizeof header);
    memcpy(bytes + sizeof header, &fallback_, sizeof fallback_);
    if (count != 0u && static_cast<const void *>(bytes + header_bytes) != static_cast<const void *>(rows))
        memcpy(bytes + header_bytes, rows, uint64_t{count} * sizeof(Row));
    return true;
}

bool GpuRangeTable::lookup(const void *table, uint64_t guest, uint64_t &host, uint64_t &room) const
{
    AV_SCOPE("GpuRangeTable::lookup");
    host = fallback_;
    room = 0u;
    if (table == nullptr) return false;
    const uint8_t *const bytes = static_cast<const uint8_t *>(table);
    uint32_t             count = 0u;
    memcpy(&count, bytes, sizeof count);
    uint32_t low = 0u, high = count;
    Row      row = {};
    while (low < high) {
        const uint32_t mid = (low + high) / 2u;
        memcpy(&row, bytes + header_bytes + uint64_t{mid} * sizeof(Row), sizeof row);
        if (row.guest_ <= guest) low = mid + 1u;
        else high = mid;
    }
    if (low == 0u) return false;
    memcpy(&row, bytes + header_bytes + uint64_t{low - 1u} * sizeof(Row), sizeof row);
    const uint64_t into = guest - row.guest_;
    if (into >= row.bytes_) return false;
    host = row.host_ + into;
    room = row.bytes_ - into;
    return true;
}

}
