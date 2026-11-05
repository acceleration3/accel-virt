#include "icd/opengl/buffer_clear_pattern.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t header = 12u;
constexpr std::size_t widest_read = 16u;

std::uint32_t word(const std::uint8_t *at)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, at, sizeof value);
    return value;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header) return 0;
    const std::uint32_t internal_format = word(data);
    const std::uint32_t format = word(data + 4);
    const std::uint32_t type = word(data + 8);
    const std::size_t   given = size - header;
    auto *const         payload = static_cast<std::uint8_t *>(std::malloc(widest_read));
    if (payload == nullptr) return 0;
    std::memset(payload, 0, widest_read);
    std::memcpy(payload, data + header, given < widest_read ? given : widest_read);
    av::glicd::BufferClearPattern pattern;
    const std::uint32_t           converted = pattern.convert(internal_format, format, type, payload);
    std::free(payload);
    if (!pattern.invariants_hold() || pattern.size() > av::glicd::BufferClearPattern::most_bytes) __builtin_trap();
    if (converted != av::glicd::BufferClearPattern::gl_no_error && pattern.size() != 0u) __builtin_trap();
    if (converted != av::glicd::BufferClearPattern::gl_no_error &&
        converted != av::glicd::BufferClearPattern::gl_invalid_enum &&
        converted != av::glicd::BufferClearPattern::gl_invalid_operation)
        __builtin_trap();
    const std::size_t room = given % 67u;
    auto *const       destination = static_cast<std::uint8_t *>(std::malloc(room != 0u ? room : 1u));
    if (destination == nullptr) return 0;
    pattern.fill(destination, room);
    std::free(destination);
    av::glicd::BufferClearPattern zero;
    if (zero.convert(internal_format, format, type, nullptr) != converted) __builtin_trap();
    if (zero.size() != pattern.size() || !zero.invariants_hold()) __builtin_trap();
    return 0;
}
