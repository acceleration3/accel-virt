/* The escape record a process hands the kernel to learn where its context's
 * ring, reply slot and fence page are mapped, fed attacker-chosen bytes (docs/protocol.md,
 * "Context memory"). What decodes must re-encode to exactly the bytes read, and
 * the kernel's answer must never decode as a request. The same bytes are then
 * a kernel's answer the process reads back: what it accepts must re-encode to
 * exactly those bytes too, and name distinct addresses: two in version 1,
 * three in version 2. Both versions are asked. */
#include "core/support/status.hpp"
#include "proto/escape/context_views_record.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    std::uint8_t again[av::ContextViewsRecord::encoded_size] = {};
    for (const std::uint16_t version : {av::ContextViewsRecord::without_fence, av::ContextViewsRecord::with_fence}) {
        av::ContextViewsRecord answer(version);
        if (answer.accept_answer(data, size) != av::Status::ok) continue;
        if (answer.ring() == 0u || answer.reply() == 0u || answer.ring() == answer.reply()) __builtin_trap();
        if (version == av::ContextViewsRecord::with_fence &&
            (answer.fence() == 0u || answer.fence() == answer.ring() || answer.fence() == answer.reply()))
            __builtin_trap();
        if (version == av::ContextViewsRecord::without_fence && answer.fence() != 0u) __builtin_trap();
        if (answer.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
        if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    }
    av::ContextViewsRecord record;
    if (record.decode(data, size) != av::Status::ok) return 0;
    if (record.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, data, sizeof again) != 0) __builtin_trap();
    const av::ContextViewsRecord answered = record.answered(0x10000u, 0x20000u, 0x30000u);
    if (answered.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    av::ContextViewsRecord reread;
    if (reread.decode(again, sizeof again) == av::Status::ok) __builtin_trap();
    return 0;
}
