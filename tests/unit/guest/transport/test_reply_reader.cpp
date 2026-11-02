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
    static void orphan(ReplyReader &reader) { reader.slot_ = nullptr; reader.capacity_ = 32u; }
};

}

namespace {
struct Slot {
    std::uint8_t bytes[64] = {};
    void answer(std::uint64_t fence, const std::uint8_t *payload, std::uint32_t size)
    {
        static_cast<void>(ReplyHeader(fence, size).encode(bytes, sizeof bytes));
        std::memcpy(bytes + ReplyHeader::encoded_size, payload, size);
    }
};

}

TEST(reply_reader, unbound_reads_nothing)
{ ReplyReader reader;
  CHECK(reader.invariants_hold());
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(1u, answer), Status::invalid_state);
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

TEST(reply_reader, a_slot_written_for_another_fence_is_refused)
{ Slot slot;
  const std::uint8_t payload[] = {1u, 2u, 3u, 4u};
  slot.answer(9u, payload, sizeof payload);
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(8u, answer), Status::not_found); }

TEST(reply_reader, an_unwritten_slot_is_refused)
{ Slot slot;
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(1u, answer), Status::malformed); }

TEST(reply_reader, an_answer_too_large_is_reported_with_the_needed_size)
{ Slot slot;
  const std::uint32_t needed = 4096u;
  static_cast<void>(ReplyHeader(5u, needed).within(sizeof slot.bytes - ReplyHeader::encoded_size)
                        .encode(slot.bytes, sizeof slot.bytes));
  ReplyReader reader(slot.bytes, sizeof slot.bytes);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(5u, answer), Status::too_large);
  CHECK_EQ(reader.required(), needed); }

TEST(reply_reader, a_slot_shorter_than_a_header_is_refused)
{ std::uint8_t tiny[16] = {};
  ReplyReader reader(tiny, sizeof tiny);
  WireReader answer(nullptr, 0u);
  CHECK_STATUS(reader.read(1u, answer), Status::malformed); }

TEST(reply_reader, an_answer_size_past_the_slot_is_refused)
{ Slot slot;
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
