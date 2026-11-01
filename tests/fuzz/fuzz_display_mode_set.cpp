/* GET_DISPLAY_MODES answers as a hostile device writes them. Whatever is
 * accepted is what the bytes at the documented offsets say, with every mode a
 * display could have and the preferred one among them; a refused answer
 * leaves the last accepted set untouched. */
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

std::uint32_t field(const std::uint8_t *data, std::size_t offset)
{
    std::uint32_t value = 0u;
    std::memcpy(&value, data + offset, sizeof value);
    return value;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::DisplayModeSet set;
    if (set.add(1280u, 800u) != av::Status::ok) __builtin_trap();
    if (set.decode(data, size) != av::Status::ok) {
        if (set.count() != 1u || set.width(0u) != 1280u || !set.invariants_hold()) __builtin_trap();
        return 0;
    }
    if (!set.invariants_hold()) __builtin_trap();
    if (field(data, 0u) != av::DisplayModeSet::response_type) __builtin_trap();
    if (set.count() != field(data, 24u) || set.preferred() != field(data, 28u)) __builtin_trap();
    for (std::uint32_t index = 0u; index < set.count(); index++) {
        if (set.width(index) != field(data, 32u + index * 8u)) __builtin_trap();
        if (set.height(index) != field(data, 36u + index * 8u)) __builtin_trap();
    }
    /* What was accepted encodes back to the same bytes, the refresh apart: a
     * zero or missing one is 60 Hz. */
    const std::size_t modes_end = av::DisplayModeSet::fixed_size + set.count() * av::DisplayModeSet::mode_size;
    if (size >= modes_end + av::DisplayModeSet::refresh_size && field(data, modes_end) != 0u &&
        set.refresh_millihz() != field(data, modes_end))
        __builtin_trap();
    std::uint8_t   again[av::DisplayModeSet::max_encoded_size] = {};
    av::WireWriter writer(again, sizeof again);
    if (set.encode(writer) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again + 24u, data + 24u, modes_end - 24u) != 0) __builtin_trap();
    return 0;
}
