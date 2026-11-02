#include "guest/inspect/command_line.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    std::uint16_t line[2048] = {};
    const std::size_t units = size / 2u < sizeof line / sizeof line[0] - 1u ? size / 2u : sizeof line / sizeof line[0] - 1u;
    for (std::size_t unit = 0u; unit < units; unit++)
        line[unit] = static_cast<std::uint16_t>(data[2u * unit] | (data[2u * unit + 1u] << 8u));
    av::CommandLine parsed;
    const av::Status status = parsed.parse(line);
    if (!parsed.invariants_hold()) __builtin_trap();
    if (status != av::Status::ok && parsed.count() != 0u) __builtin_trap();
    std::size_t total = 0u;
    for (std::uint32_t index = 0u; index <= parsed.count(); index++) {
        total += std::strlen(parsed.argument(index));
        std::uint32_t value = 0u;
        static_cast<void>(parsed.number(index, value));
    }
    if (total > av::CommandLine::max_bytes) __builtin_trap();
    return 0;
}
