/* The reply slot a context reads its synchronous answer back from. The reader
 * refuses a slot the device has not written for this fence - stale bytes, a
 * fence that is not the one awaited, an answer too large for the slot - and
 * otherwise hands back a reader over exactly the answer's bytes. The answer is
 * only valid until the next send overwrites the slot (contract B2). */
#include "core/support/status.hpp"
#include "guest/transport/reply_reader.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/submission/reply_header.hpp"
#include "proto/wire/wire_reader.hpp"

#include <cstdint>
#include <cstring>

using av::ReplyHeader;
using av::ReplyReader;
using av::Status;
using av::WireReader;
using av::ReplyReaderProbe;

namespace av {

struct ReplyReaderProbe {
    /* A slot that is gone with a capacity left behind: what the class never
     * builds and its invariant catches. */
    static void orphan(ReplyReader &reader) { reader.slot_ = nullptr; reader.capacity_ = 32u; }
};

}  // namespace av

namespace {

/* A reply slot big enough for a small answer. */
struct Slot {
    std::uint8_t bytes[64] = {};
    /* Writes the header for `fence` with `size` bytes, then the answer. */
    void answer(std::uint64_t fence, const std::uint8_t *payload, std::uint32_t size)
    {
        static_cast<void>(ReplyHeader(fence, size).encode(bytes, sizeof bytes));
        std::memcpy(bytes + ReplyHeader::encoded_size, payload, size);
    }
};

}  // namespace

TEST(reply_reader, unbound_reads_nothing)
{ ReplyReader reader;
  CHECK(reader.invariants_hold());
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(1u, answer), Status::invalid_state);
  /* A null slot has no capacity, whatever the caller passed. */
  ReplyReader from_null(nullptr, 64u);
  CHECK(from_null.invariants_hold());
  CHECK_STATUS(from_null.read(1u, answer), Status::invalid_state); }

TEST(reply_reader, hands_back_the_answer_the_device_wrote_for_the_fence)
{ Slot slot;
  const std::uint8_t payload[] = {0x11u, 0x22u, 0x33u, 0x44u, 0x55u, 0x66u, 0x77u, 0x88u};
  slot.answer(7u, payload, sizeof payload);
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(7u, answer), Status::ok);
  CHECK_EQ(answer.read_u32(), 0x44332211u);
  CHECK_EQ(answer.read_u32(), 0x88776655u);
  CHECK_STATUS(answer.status(), Status::ok); }

/* An answer for another fence is not this call's answer. */
TEST(reply_reader, a_slot_written_for_another_fence_is_refused)
{ Slot slot;
  const std::uint8_t payload[] = {1u, 2u, 3u, 4u};
  slot.answer(9u, payload, sizeof payload);
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(8u, answer), Status::not_found); }

/* A slot the device never wrote has no magic and is refused. */
TEST(reply_reader, an_unwritten_slot_is_refused)
{ Slot slot;   /* all zero */
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(1u, answer), Status::malformed); }

/* When the answer did not fit the slot the device says so; the reader reports
 * how much was needed and hands back no bytes. */
TEST(reply_reader, an_answer_too_large_is_reported_with_the_needed_size)
{ Slot slot;
  const std::uint32_t needed = 4096u;
  static_cast<void>(ReplyHeader(5u, needed).within(sizeof slot.bytes - ReplyHeader::encoded_size)
                        .encode(slot.bytes, sizeof slot.bytes));
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(5u, answer), Status::too_large);
  CHECK_EQ(reader.required(), needed); }

/* A slot too small to hold even a header cannot answer. */
TEST(reply_reader, a_slot_shorter_than_a_header_is_refused)
{ std::uint8_t tiny[16] = {};
  ReplyReader reader(tiny, sizeof tiny);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(1u, answer), Status::malformed); }

/* An answer whose size overruns the slot is not trusted. */
TEST(reply_reader, an_answer_size_past_the_slot_is_refused)
{ Slot slot;
  /* A header claiming 4096 ok bytes in a 32-byte answer area. */
  static_cast<void>(ReplyHeader(3u, 4096u).encode(slot.bytes, sizeof slot.bytes));
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(3u, answer), Status::malformed); }

TEST(reply_reader, the_invariant_catches_a_slot_gone_with_a_capacity_left)
{ std::uint8_t bytes[64] = {};
  ReplyReader reader(bytes, sizeof bytes);
  CHECK(reader.invariants_hold());
  ReplyReaderProbe::orphan(reader);
  CHECK(!reader.invariants_hold()); }
