#include "core/support/status.hpp"
#include "icd/opengl/pixel_format_query.hpp"
#include "icd/opengl/pixel_format_table.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {
constexpr std::size_t list_slots = 2u * av::glicd::PixelFormatQuery::max_pairs + 3u;
constexpr std::size_t header_bytes = 12u;

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header_bytes) return 0;
    std::int32_t format = 0;
    std::uint32_t max = 0u;
    std::uint32_t count = 0u;
    std::memcpy(&format, data, 4u);
    std::memcpy(&max, data + 4u, 4u);
    std::memcpy(&count, data + 8u, 4u);
    max %= 40u;
    count %= 140u;
    std::int32_t ints[list_slots] = {};
    float        floats[list_slots] = {};
    const std::size_t rest = size - header_bytes;
    const std::size_t int_bytes = rest / 2u < sizeof ints ? rest / 2u : sizeof ints;
    std::memcpy(ints, data + header_bytes, int_bytes);
    const std::size_t float_bytes = rest - rest / 2u < sizeof floats ? rest - rest / 2u : sizeof floats;
    std::memcpy(floats, data + header_bytes + rest / 2u, float_bytes);

    const av::glicd::PixelFormatTable table;
    const av::glicd::PixelFormatQuery query(table);
    std::int32_t  formats[41];
    for (std::int32_t &slot : formats) slot = -1;
    std::uint32_t matched = 0u;
    if (query.choose(ints, floats, max, formats, &matched) == av::Status::ok) {
        if (matched > table.count()) __builtin_trap();
        for (std::uint32_t at = 0u; at < max && at < matched; at++)
            if (!table.contains(formats[at])) __builtin_trap();
    }
    if (formats[max] != -1) __builtin_trap();
    std::int32_t values[141];
    for (std::int32_t &value : values) value = -1;
    const av::Status described = query.attribs(format, 0, count, ints, values, nullptr);
    if (described != av::Status::ok) {
        for (const std::int32_t value : values)
            if (value != -1) __builtin_trap();
    }
    if (values[count] != -1) __builtin_trap();
    if (!query.invariants_hold()) __builtin_trap();
    return 0;
}
