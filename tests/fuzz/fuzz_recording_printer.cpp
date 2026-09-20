/* A wire recording printed as anything at all. The printer walks a recording's
 * records and, inside a batch record, the guest's own framed commands - bytes
 * neither the device nor the reader wrote - so every line it produces must be
 * a terminated string it built inside its own buffer, from payload bytes
 * inside the recording (ASan), for any input at all.
 *
 * Printed twice: with no payload bytes shown, and with as many as a line may
 * hold under a filter that reads every command's name, so both the widest
 * line and the command walk are driven.
 *
 * Input: the recording's bytes, from its file header on. */
#include "core/support/status.hpp"
#include "device/recording/recording_filter.hpp"
#include "device/recording/recording_printer.hpp"
#include "device/recording/recording_reader.hpp"

#include <cstddef>
#include <cstdint>

namespace {

void printed(const std::uint8_t *data, std::size_t size, const av::RecordingFilter &filter,
             std::uint32_t payload_bytes)
{
    av::RecordingReader  reader(data, size);
    av::RecordingPrinter printer(reader, filter, payload_bytes);
    if (!printer.invariants_hold()) __builtin_trap();
    /* A line costs a record header or a command header, so a recording of
     * `size` bytes can produce no more than this many; a printer that answered
     * more would be one that did not advance. Plus the damage line. */
    const std::uint64_t most = size + 2u;
    for (std::uint64_t lines = 0u;; lines++) {
        if (lines > most) __builtin_trap();
        const char  *line = nullptr;
        const av::Status status = printer.next(line);
        if (status == av::Status::not_found) break;
        if (status != av::Status::ok || line == nullptr) __builtin_trap();
        /* Terminated inside the printer's own line, and never empty: a line
         * nobody can read is not a line. */
        std::size_t characters = 0u;
        while (characters < av::RecordingPrinter::line_bytes && line[characters] != '\0') characters++;
        if (characters == 0u || characters >= av::RecordingPrinter::line_bytes) __builtin_trap();
        if (!printer.invariants_hold()) __builtin_trap();
        if (printer.lines() != lines + 1u) __builtin_trap();
    }
    if (!printer.invariants_hold()) __builtin_trap();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const av::RecordingFilter everything;
    printed(data, size, everything, 0u);

    av::RecordingFilter named;
    /* Every generated name holds a lower-case letter, so this reaches the
     * name lookup of every command a batch holds. */
    if (named.keep_command("e") != av::Status::ok) __builtin_trap();
    printed(data, size, named, av::RecordingPrinter::most_payload_bytes);
    return 0;
}
