/* The private data a present carries between the drivers, fed
 * attacker-chosen bytes: a present snapshot's key as the miniport reads it
 * from a process's DdiPresent, and the miniport's UMDRIVERPRIVATE answer as a
 * user-mode driver reads it. What decodes re-encodes to the bytes read; a
 * refusal leaves the reader as it was. */
#include "core/support/status.hpp"
#include "proto/context/umd_adapter_info.hpp"
#include "proto/submission/present_snapshot_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::PresentSnapshotRecord record(5u);
    if (record.decode(data, size) == av::Status::ok) {
        std::uint8_t again[av::PresentSnapshotRecord::encoded_size] = {};
        if (record.key() == 0u || record.encode(again, sizeof again) != av::Status::ok ||
            std::memcmp(again, data, sizeof again) != 0)
            __builtin_trap();
    } else if (record.key() != 5u) {
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
