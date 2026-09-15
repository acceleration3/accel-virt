/* The escape record a process hands the kernel to bind a host-visible chunk,
 * fed attacker-chosen bytes (feature 045). What decodes must re-encode to exactly
 * the bytes read, and the kernel's answer to it must never decode as a request.
 * The same bytes are then a kernel's answer the process reads back: what a bind
 * or an unbind accepts must re-encode to exactly those bytes too. */
#include "core/support/status.hpp"
#include "proto/resources/host_visible_bind_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    using Operation = av::HostVisibleBindRecord::Operation;
    std::uint8_t again[av::HostVisibleBindRecord::encoded_size] = {};
    const av::HostVisibleBindRecord requests[] = {av::HostVisibleBindRecord(Operation::bind, 0x40000000u, 0u),
                                                  av::HostVisibleBindRecord(Operation::unbind, 0u, 0x00100003u)};
    for (av::HostVisibleBindRecord request : requests) {
        if (request.accept_answer(data, size) != av::Status::ok) continue;
        if (request.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    }
    av::HostVisibleBindRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    const av::HostVisibleBindRecord answer = record.answered(record.resource_id() + 1u, 1u);
    if (answer.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    av::HostVisibleBindRecord reread;
    if (reread.decode(again, sizeof again) == av::Status::ok) __builtin_trap();
    return 0;
}
