/* Hostile trace escapes as the driver receives them from a process, and
 * hostile answers as rvkinspect.exe receives them from a driver: a request is
 * decoded or refused, and an accepted one never names a window past its bytes;
 * an accepted answer's window lies inside both its bytes and its total. */
#include "proto/trace/trace_escape_record.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    av::TraceEscapeRecord request;
    if (request.decode(data, size) == av::Status::ok) {
        if (request.window() > size - av::TraceEscapeRecord::encoded_size) __builtin_trap();
        if (request.total() != 0u || request.clock() != 0u) __builtin_trap();
    }
    av::TraceEscapeRecord asked(av::TraceEscapeRecord::Kind::table, 0u, 4096u);
    if (asked.accept_answer(data, size) == av::Status::ok) {
        if (asked.window() > size - av::TraceEscapeRecord::encoded_size || asked.window() > 4096u) __builtin_trap();
        if (asked.offset() > asked.total() || asked.window() > asked.total() - asked.offset()) __builtin_trap();
    }
    return 0;
}
