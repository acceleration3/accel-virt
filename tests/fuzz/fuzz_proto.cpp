/* Every rvk protocol decoder, fed attacker-chosen bytes. The first byte picks
 * the target. A decoder may reject anything; what it accepts must re-encode to
 * exactly the bytes it read, and the trackers must stay consistent whatever
 * sequence of snapshots they see. */
#include "../../src/core/status.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../../src/proto/capset_info.hpp"
#include "../../src/proto/context_setup.hpp"
#include "../../src/proto/fence_page.hpp"
#include "../../src/proto/fence_progress.hpp"
#include "../../src/proto/ring_header.hpp"
#include "../../src/proto/ring_progress.hpp"
#include "../../src/proto/submission_descriptor.hpp"
#include "../../src/proto/wire_reader.hpp"

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
    switch (data[0] % 8u) {
    case 0: round_trip<av::CapsetInfo>(payload, length); break;
    case 1: round_trip<av::ContextSetup>(payload, length); break;
    case 2: round_trip<av::RingHeader>(payload, length); break;
    case 3: round_trip<av::SubmissionDescriptor>(payload, length); break;
    case 4: round_trip<av::FencePage>(payload, length); break;
    case 5: ring_sequence(payload, length); break;
    case 6: fence_sequence(payload, length); break;
    default: round_trip<av::AllocationDescriptor>(payload, length); break;
    }
    return 0;
}
