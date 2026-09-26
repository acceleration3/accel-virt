/* Every rvk protocol decoder, fed attacker-chosen bytes. The first byte picks
 * the target. A decoder may reject anything; what it accepts must re-encode to
 * exactly the bytes it read, and the trackers must stay consistent whatever
 * sequence of snapshots they see. */
#include "core/support/status.hpp"
#include "proto/context/capset_info.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/fence_progress.hpp"
#include "proto/submission/ring_header.hpp"
#include "proto/submission/ring_progress.hpp"
#include "proto/submission/submission_descriptor.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

template <typename Structure>
void round_trip(const std::uint8_t *bytes, std::size_t length)
{
    Structure decoded;
    if (decoded.decode(bytes, length) != av::Status::ok) return;
    std::uint8_t again[Structure::encoded_size] = {};
    if (decoded.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, bytes, sizeof again) != 0) __builtin_trap();
}

/* The header's awake flag and published fence are the device's and the
 * miniport's to write; the process's encode leaves them zero, so only the
 * rest of what decoded comes back. */
void ring_header_round_trip(const std::uint8_t *bytes, std::size_t length)
{
    constexpr std::size_t awake_flag = 5u;
    constexpr std::size_t published_fence = 24u;
    av::RingHeader decoded;
    if (decoded.decode(bytes, length) != av::Status::ok) return;
    std::uint8_t again[av::RingHeader::encoded_size] = {};
    if (decoded.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    std::uint8_t expected[av::RingHeader::encoded_size] = {};
    std::memcpy(expected, bytes, sizeof expected);
    expected[awake_flag] = 0u;
    std::memset(expected + published_fence, 0, sizeof expected - published_fence);
    if (std::memcmp(again, expected, sizeof again) != 0) __builtin_trap();
}

/* A setup is 32 or 48 bytes by its minor: what decodes is exactly its size. */
void setup_round_trip(const std::uint8_t *bytes, std::size_t length)
{
    av::ContextSetup decoded;
    if (decoded.decode(bytes, length) != av::Status::ok) return;
    if (decoded.size() != length) __builtin_trap();
    std::uint8_t again[av::ContextSetup::encoded_size] = {};
    if (decoded.encode(again, sizeof again) != av::Status::ok) __builtin_trap();
    if (std::memcmp(again, bytes, length) != 0) __builtin_trap();
    if (decoded.sub_api() != av::SubApi::opengl && !decoded.options().defaults()) __builtin_trap();
}

void ring_sequence(const std::uint8_t *bytes, std::size_t length)
{
    av::WireReader reader(bytes, length);
    av::RingProgress progress;
    while (reader.remaining() >= 17u) {
        const std::uint8_t order = reader.read_u8();
        const std::uint64_t tail = reader.read_u64();
        const std::uint64_t head = reader.read_u64();
        (void)progress.observe(av::RingHeader(order, tail, head));
        if (!progress.invariants_hold()) __builtin_trap();
    }
}

void fence_sequence(const std::uint8_t *bytes, std::size_t length)
{
    av::WireReader reader(bytes, length);
    av::FenceProgress progress;
    while (reader.remaining() >= 1u + av::FencePage::encoded_size) {
        const std::uint8_t kind = reader.read_u8();
        if ((kind & 1u) != 0u) {
            (void)progress.record_submission(reader.read_u64());
        } else {
            std::uint8_t page_bytes[av::FencePage::encoded_size] = {};
            reader.copy_bytes(page_bytes, sizeof page_bytes);
            av::FencePage page;
            if (page.decode(page_bytes, sizeof page_bytes) == av::Status::ok)
                (void)progress.observe(page);
        }
        if (!progress.invariants_hold()) __builtin_trap();
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    if (size == 0u) return 0;
    const std::uint8_t *payload = data + 1;
    const std::size_t length = size - 1u;
    switch (data[0] % 11u) {
    case 0: round_trip<av::CapsetInfo>(payload, length); break;
    case 1: setup_round_trip(payload, length); break;
    case 2: ring_header_round_trip(payload, length); break;
    case 3: round_trip<av::SubmissionDescriptor>(payload, length); break;
    case 4: round_trip<av::FencePage>(payload, length); break;
    case 5: ring_sequence(payload, length); break;
    case 6: fence_sequence(payload, length); break;
    case 8: round_trip<av::BatchHeader>(payload, length); break;
    case 9: round_trip<av::CommandHeader>(payload, length); break;
    case 10: round_trip<av::ReplyHeader>(payload, length); break;
    default: round_trip<av::AllocationDescriptor>(payload, length); break;
    }
    return 0;
}
