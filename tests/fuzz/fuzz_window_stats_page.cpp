#include "core/support/status.hpp"
#include "device/memory/window_stats_window.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/pages/window_stats.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {
constexpr std::uint64_t ram = 0x100000u;
constexpr std::uint64_t page = ram + 0x1000u;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::WindowStats stats;
    if (stats.decode(data, size) == av::Status::ok) {
        if (size < av::WindowStats::encoded_size) __builtin_trap();
        std::uint8_t again[av::WindowStats::encoded_size] = {};
        if (stats.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, av::WindowStats::sequence_at) != 0) __builtin_trap();
        if (std::memcmp(again + av::WindowStats::fields_at, data + av::WindowStats::fields_at,
                        av::WindowStats::encoded_size - av::WindowStats::fields_at) != 0)
            __builtin_trap();
    }
    av::MockHost host;
    host.add_guest_ram(ram, 0x4000u);
    std::memset(host.guest_ram(page), 0, av::WindowStats::page_size);
    std::memcpy(host.guest_ram(page), data, size < av::WindowStats::page_size ? size : av::WindowStats::page_size);
    av::WindowStatsWindow window(host);
    const av::Status      mapped = window.map(page);
    if (mapped == av::Status::ok) {
        if (!window.mapped() || !window.invariants_hold()) __builtin_trap();
        av::WindowStats read;
        const av::Status      found = window.read(read);
        if (found != av::Status::ok && found != av::Status::conflict) __builtin_trap();
        window.release();
    } else if (window.mapped()) {
        __builtin_trap();
    }
    if (host.live_guest_mappings() != 0u) __builtin_trap();
    return 0;
}
