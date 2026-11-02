#include "core/support/status.hpp"
#include "proto/escape/umd_adapter_info.hpp"
#include "proto/escape/present_point_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::PresentPointRecord record(5u);
    if (record.decode(data, size) == av::Status::ok) {
        std::uint8_t again[av::PresentPointRecord::encoded_size] = {};
        if (record.point() == 0u || record.encode(again, sizeof again) != av::Status::ok ||
            std::memcmp(again, data, sizeof again) != 0)
            __builtin_trap();
    } else if (record.point() != 5u) {
        __builtin_trap();
    }
    av::UmdAdapterInfo info(6u, 0u);
    if (info.decode(data, size) != av::Status::ok) {
        if (info.luid() != 6u || info.offers(~std::uint64_t{0})) __builtin_trap();
        return 0;
    }
    std::uint8_t again[av::UmdAdapterInfo::encoded_size] = {};
    std::size_t  written = 0u;
    const std::size_t room = size >= av::UmdAdapterInfo::encoded_size ? sizeof again : av::UmdAdapterInfo::base_size;
    if (info.encode(again, room, written) != av::Status::ok || written != room) __builtin_trap();
    if (std::memcmp(again, data, written) != 0) __builtin_trap();
    return 0;
}
