/* The present engine over an attacker-chosen queued present: the fixed part,
 * the page lists and the regions are all the guest's. Whatever it asks, the
 * device answers once, the ticket completes - refused unless it was taken -
 * nothing stays mapped once the engine is reset, and no byte of guest RAM
 * changes outside the page and the destination's own entries; a request that
 * does not decode changes none of them. */
#include "device/control/control_request.hpp"
#include "device/control/present_engine_commands.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/present/present_engine.hpp"
#include "device/present/present_snapshots.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/display/present_progress.hpp"
#include "proto/virtio_gpu/queued_present_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <time.h>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::size_t   ram_bytes = 0x10000u;
constexpr std::uint64_t page = ram + 0xE000u;
/* A region boundary in the middle, as QEMU's RAM blocks split a mapping. */
constexpr std::uint64_t split = ram + 0x7000u;
constexpr std::uint32_t ok_nodata = 0x1100u;

bool in_destination(const std::uint8_t *data, std::size_t size, const av::QueuedPresentRequest &request,
                    std::uint64_t address)
{
    for (std::uint32_t entry = 0u; entry < request.copy().destination_entries(); entry++) {
        const std::size_t at = request.destination_entries_offset() + entry * av::QueuedPresentRequest::entry_size;
        if (at + av::QueuedPresentRequest::entry_size > size) return false;
        std::uint64_t start = 0u;
        std::uint32_t length = 0u;
        std::memcpy(&start, data + at, sizeof start);
        std::memcpy(&length, data + at + 8u, sizeof length);
        if (address >= start && address - start < length) return true;
    }
    return false;
}

/* The page's progress once it says `ticket` is done: false when it never
 * does within a second. */
bool completes(av::MockHost &host, std::uint64_t ticket, av::PresentProgress &progress)
{
    const timespec pause = {0, 20000};
    for (std::uint32_t waited = 0u; waited < 50000u; waited++) {
        if (progress.decode(host.guest_ram(page), av::PresentProgress::encoded_size) == av::Status::ok &&
            progress.completed() >= ticket)
            return true;
        nanosleep(&pause, nullptr);
    }
    return false;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockHost host;
    host.add_guest_ram(ram, ram_bytes);
    host.split_guest_memory_at(split);
    std::uint8_t *const memory = host.guest_ram(ram);
    for (std::size_t at = 0u; at < ram_bytes; at++) memory[at] = static_cast<std::uint8_t>(at * 13u + 5u);

    av::FenceInterrupts interrupts(host);
    av::PresentEngine   engine(host, interrupts);
    const av::PresentSnapshots none;
    if (engine.initialize() != av::Status::ok || engine.attach(page, none) != av::Status::ok) __builtin_trap();
    std::uint32_t response = 0u;
    {
        av::MockRequest    mock(data, size);
        av::ControlRequest request(host, &mock);
        av::PresentEngineCommands commands(host, engine, none);
        if (!commands.answer(request, av::QueuedPresentRequest::type) || !mock.answered()) __builtin_trap();
        response = mock.response_type();
    }
    av::PresentProgress progress;
    if (!completes(host, 1u, progress) || progress.completed() != 1u || progress.refused() > 1u) __builtin_trap();
    if (response != ok_nodata && progress.refused() != 1u) __builtin_trap();
    engine.detach();
    if (host.live_guest_mappings() != 0u || host.live_threads() != 0u) __builtin_trap();

    av::WireReader           reader(data, size);
    av::QueuedPresentRequest request;
    const bool               taken = response == ok_nodata && request.decode(reader) == av::Status::ok;
    for (std::size_t at = 0u; at < ram_bytes; at++) {
        if (ram + at >= page && ram + at < page + av::PresentProgress::page_size) continue;
        if (memory[at] == static_cast<std::uint8_t>(at * 13u + 5u)) continue;
        if (!taken || !in_destination(data, size, request, ram + at)) __builtin_trap();
    }
    return 0;
}
