#include "miniport/adapter/adapter.hpp"
#include "miniport/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "miniport/ddi/dxgkarg_escape_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "proto/escape/trace_escape_record.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::size_t   header_bytes = 13u;
constexpr std::uint32_t max_buffer = 0x400u;
constexpr std::size_t   guard_bytes = 16u;
constexpr std::uint8_t  poison = 0x5Au;

std::uint32_t read_u32(const std::uint8_t *bytes)
{
    return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8u |
           static_cast<std::uint32_t>(bytes[2]) << 16u | static_cast<std::uint32_t>(bytes[3]) << 24u;
}

bool accepted(std::int32_t status)
{
    const auto value = static_cast<std::uint32_t>(status);
    return value == 0u || value == 0xC0000002u || value == 0xC000000Du || value == 0xC0000017u;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size < header_bytes) return 0;
    const std::int32_t  type = static_cast<std::int32_t>(read_u32(data));
    const std::uint32_t input_size = read_u32(data + 4u) % (max_buffer + 1u);
    const std::uint32_t output_size = read_u32(data + 8u) % (max_buffer + 1u);
    const std::uint8_t  selector = data[12];

    std::uint8_t input[max_buffer];
    std::uint8_t output[max_buffer + guard_bytes];
    std::memset(input, 0, sizeof input);
    std::memcpy(input, data + header_bytes, size - header_bytes < max_buffer ? size - header_bytes : max_buffer);
    std::memset(output, poison, sizeof output);

    av::MockPlatform platform;
    int physical_device_object = 0;
    {
        av::Adapter adapter(platform, &physical_device_object);
        const bool null_input = (selector & 0x10u) != 0u;
        const bool null_output = (selector & 0x20u) != 0u;
        std::int32_t status = 0;
        switch (selector % 3u) {
        case 0u: {
            av::DxgkargQueryAdapterInfoRecord query = {};
            query.Type = type;
            query.pInputData = null_input ? nullptr : input;
            query.InputDataSize = input_size;
            query.pOutputData = null_output ? nullptr : output;
            query.OutputDataSize = output_size;
            status = adapter.query_adapter_info(&query);
            if (!accepted(status)) __builtin_trap();
            break;
        }
        case 1u: {
            if ((selector & 0x40u) != 0u) {
                const std::uint32_t magic = av::TraceEscapeRecord::magic;
                std::memcpy(input, &magic, sizeof magic);
                input[4] = 1u;
                input[5] = 0u;
            }
            av::DxgkargEscapeRecord escape = {};
            escape.pPrivateDriverData = null_input ? nullptr : input;
            escape.PrivateDriverDataSize = input_size;
            status = adapter.escape(&escape);
            break;
        }
        default: {
            av::DxgkargCollectDbgInfoRecord collect = {};
            collect.pBuffer = null_output ? nullptr : output;
            collect.BufferSize = output_size;
            status = adapter.collect_dbg_info(&collect);
            break;
        }
        }
        static_cast<void>(status);
        if (!adapter.invariants_hold()) __builtin_trap();
    }
    for (std::size_t byte = output_size; byte < sizeof output; byte++)
        if (output[byte] != poison) __builtin_trap();
    if (platform.unclaimed_violations() != 0u || platform.live_allocations() != 0u) __builtin_trap();
    return 0;
}
