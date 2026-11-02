#include "device/control/control_request.hpp"
#include "device/control/present_commands.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/host/mock_request.hpp"
#include "proto/virtio_gpu/present_copy_request.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {

constexpr std::uint64_t ram = 0x100000u;
constexpr std::size_t   ram_bytes = 0x10000u;
constexpr std::uint64_t split = ram + 0x7000u;

bool in_destination(const std::uint8_t *data, std::size_t size, const av::PresentCopyRequest &request,
                    std::uint64_t address)
{
    for (std::uint32_t entry = 0u; entry < request.destination_entries(); entry++) {
        const std::size_t at = request.destination_entries_offset() + entry * av::PresentCopyRequest::entry_size;
        if (at + av::PresentCopyRequest::entry_size > size) return false;
        std::uint64_t start = 0u;
        std::uint32_t length = 0u;
        std::memcpy(&start, data + at, sizeof start);
        std::memcpy(&length, data + at + 8u, sizeof length);
        if (address >= start && address - start < length) return true;
    }
    return false;
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::MockHost host;
    host.add_guest_ram(ram, ram_bytes);
    host.split_guest_memory_at(split);
    std::uint8_t *const memory = host.guest_ram(ram);
    for (std::size_t at = 0u; at < ram_bytes; at++) memory[at] = static_cast<std::uint8_t>(at * 13u + 5u);

    av::PresentCommands commands(host);
    {
        av::MockRequest    mock(data, size);
        av::ControlRequest request(host, &mock);
        if (!commands.answer(request, av::PresentCopyRequest::type) || !mock.answered()) __builtin_trap();
    }
    if (host.live_guest_mappings() != 0u) __builtin_trap();

    av::WireReader          reader(data, size);
    av::PresentCopyRequest  request;
    const bool              decoded = request.decode(reader) == av::Status::ok;
    for (std::size_t at = 0u; at < ram_bytes; at++) {
        if (memory[at] == static_cast<std::uint8_t>(at * 13u + 5u)) continue;
        if (!decoded || !in_destination(data, size, request, ram + at)) __builtin_trap();
    }
    return 0;
}
