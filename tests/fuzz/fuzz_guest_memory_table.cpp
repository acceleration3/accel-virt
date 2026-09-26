/* Guest extents and control requests as QEMU forwards them to the renderer,
 * which a guest steers: an extent that resolves lies wholly inside one mapped
 * region and locates back to the offset it named, and a forwarded request is
 * never read past its end. */
#include "core/support/status.hpp"
#include "device/renderer/forwarded_request.hpp"
#include "device/renderer/guest_memory_table.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint32_t regions = 3u;
constexpr std::uint64_t region_bytes = 0x1000u;

std::uint64_t field(const std::uint8_t *data, std::size_t offset)
{
    std::uint64_t value = 0u;
    std::memcpy(&value, data + offset, sizeof value);
    return value;
}

void check_extent(const av::GuestMemoryTable &table, const std::uint8_t *memory, std::uint64_t encoded,
                  std::uint64_t length)
{
    const auto *const pointer = static_cast<const std::uint8_t *>(table.resolve(encoded, length));
    if (pointer == nullptr) return;
    const std::uint64_t region = encoded >> av::GuestMemoryTable::region_shift;
    const std::uint64_t offset = encoded & (av::GuestMemoryTable::max_region_size - 1u);
    if (region >= regions || length == 0u || offset + length > region_bytes) __builtin_trap();
    if (pointer != memory + region * region_bytes + offset) __builtin_trap();
    int           descriptor = -1;
    std::uint64_t located = 0u;
    if (table.locate(pointer, length, descriptor, located) != av::Status::ok) __builtin_trap();
    if (descriptor != static_cast<int>(region) + 10 || located != offset) __builtin_trap();
}

void check_request(const std::uint8_t *data, std::size_t size, std::size_t offset, std::size_t want)
{
    std::uint8_t          buffer[64] = {};
    const std::size_t     bounded = want < sizeof buffer ? want : sizeof buffer;
    av::ForwardedRequest  request(data, size);
    const std::size_t     count = request.read(offset, buffer, bounded);
    const std::size_t     available = offset < size ? size - offset : 0u;
    if (count != (bounded < available ? bounded : available)) __builtin_trap();
    if (count != 0u && std::memcmp(buffer, data + offset, count) != 0) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    alignas(64) static std::uint8_t memory[regions * region_bytes];
    av::GuestMemoryTable            table;
    for (std::uint32_t region = 0u; region < regions; region++)
        if (table.add(region, memory + region * region_bytes, region_bytes, static_cast<int>(region) + 10) !=
            av::Status::ok)
            __builtin_trap();
    if (size < 16u) return 0;
    check_extent(table, memory, field(data, 0u), field(data, 8u));
    check_request(data, size, static_cast<std::size_t>(field(data, 0u) & 0xFFu),
                  static_cast<std::size_t>(field(data, 8u) & 0xFFu));
    if (!table.invariants_hold()) __builtin_trap();
    return 0;
}
