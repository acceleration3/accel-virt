#include "core/support/status.hpp"
#include "proto/resources/shared_object_kind.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::SharedObjectRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;

    std::uint8_t again[av::SharedObjectRecord::encoded_size] = {};
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    if (!record.kind().known()) __builtin_trap();
    if ((record.object_id() == 0u) != (record.adapter_id() == 0u)) __builtin_trap();
    if (record.kind().is_fence()) {
        if (record.size() != 0u) __builtin_trap();
    } else if (record.size() == 0u || record.size() % av::SharedObjectRecord::page_size != 0u) {
        __builtin_trap();
    }

    av::SharedObjectRecord minted = record;
    minted.assign(record.object_id() | 1u, record.adapter_id() | 1u);
    if (!minted.assigned()) __builtin_trap();
    if (minted.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    av::SharedObjectRecord reread;
    if (reread.decode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (reread.object_id() != minted.object_id() || reread.size() != record.size()) __builtin_trap();
    if (!reread.kind().same_as(record.kind())) __builtin_trap();

    if (record.decode(data, sizeof again - 1u) == av::Status::ok) __builtin_trap();
    return 0;
}
