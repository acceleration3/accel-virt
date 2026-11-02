#include "core/support/status.hpp"
#include "device/replay/recording_filter.hpp"
#include "device/replay/recording_printer.hpp"
#include "device/replay/recording_reader.hpp"

#include <cstddef>
#include <cstdint>

namespace {

void printed(const std::uint8_t *data, std::size_t size, const av::RecordingFilter &filter,
             std::uint32_t payload_bytes)
{
    av::RecordingReader  reader(data, size);
    av::RecordingPrinter printer(reader, filter, payload_bytes);
    if (!printer.invariants_hold()) __builtin_trap();
    const std::uint64_t most = size + 2u;
    for (std::uint64_t lines = 0u;; lines++) {
        if (lines > most) __builtin_trap();
        const char  *line = nullptr;
        const av::Status status = printer.next(line);
        if (status == av::Status::not_found) break;
        if (status != av::Status::ok || line == nullptr) __builtin_trap();
        std::size_t characters = 0u;
        while (characters < av::RecordingPrinter::line_bytes && line[characters] != '\0') characters++;
        if (characters == 0u || characters >= av::RecordingPrinter::line_bytes) __builtin_trap();
        if (!printer.invariants_hold()) __builtin_trap();
        if (printer.lines() != lines + 1u) __builtin_trap();
    }
    if (!printer.invariants_hold()) __builtin_trap();
}

}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const av::RecordingFilter everything;
    printed(data, size, everything, 0u);

    av::RecordingFilter named;
    if (named.keep_command("e") != av::Status::ok) __builtin_trap();
    printed(data, size, named, av::RecordingPrinter::most_payload_bytes);
    return 0;
}
