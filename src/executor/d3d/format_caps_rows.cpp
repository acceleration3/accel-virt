#include "executor/d3d/format_caps_rows.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

FormatCapsRows::FormatCapsRows() : rows_{}
{
    AV_SCOPE("FormatCapsRows::FormatCapsRows");
}

void FormatCapsRows::answer(uint32_t format, const d3dproto::FormatCaps &row)
{
    AV_SCOPE("FormatCapsRows::answer");
    if (format >= DxgiFormat::count) return;
    rows_[format] = row;
}

uint64_t FormatCapsRows::give(uint8_t *into, uint64_t capacity) const
{
    AV_SCOPE("FormatCapsRows::give");
    const uint64_t whole = capacity < sizeof(rows_) ? capacity / sizeof(rows_[0]) * sizeof(rows_[0]) : sizeof(rows_);
    if (whole != 0u) __builtin_memcpy(into, rows_, whole);
    return whole;
}

}
