#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "device/replay/recording_reader.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/submission/fault_code.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AtCeiling;
using av::BlobAction;
using av::FaultCode;
using av::GpuIdentity;
using av::MockHost;
using av::ProtocolVersion;
using av::RecordHeader;
using av::RecordingHeader;
using av::RecordingReader;
using av::RecordKind;
using av::Status;
using av::WireRecorder;

namespace {

constexpr const char *path = "read.av";
constexpr std::size_t widest = 4096u;

class Written {
public:
    Written() : host_(), recorder_(host_), length_(0u), bytes_{}
    {
        const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x21u, 0x22u};
        CHECK_STATUS(recorder_.open(path, 0u, "all", AtCeiling::stop,
                                    GpuIdentity(0x1002u, 0x73DFu, GpuIdentity::discrete_gpu, uuid, 0u, 0u,
                                                "mock gpu"),
                                    ProtocolVersion(1u, 1u)),
                     Status::ok);
        const std::uint8_t batch[] = {0xA0u, 0xA1u, 0xA2u, 0xA3u, 0xA4u};
        const std::uint8_t answer[] = {0xB0u, 0xB1u};
        recorder_.record_batch(4u, 1u, batch, sizeof batch);
        recorder_.record_reply(4u, 1u, answer, sizeof answer);
        recorder_.record_fence(4u, 1u);
        recorder_.record_fault(4u, 2u, FaultCode::device_lost);
        recorder_.record_blob(0u, BlobAction::created, 0u, 9u, 0u, 4096u);
        recorder_.close();
        length_ = host_.recording_length(path);
        CHECK(length_ <= sizeof bytes_);
        if (length_ <= sizeof bytes_) std::memcpy(bytes_, host_.recording_bytes(path), length_);
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::size_t         length() const { return length_; }

private:
    MockHost     host_;
    WireRecorder recorder_;
    std::size_t  length_;
    std::uint8_t bytes_[widest];
};

std::uint32_t walked(const std::uint8_t *bytes, std::size_t length, bool &damaged)
{
    RecordingReader reader(bytes, length);
    damaged = false;
    if (reader.open() != Status::ok) {
        damaged = true;
        CHECK(reader.damaged());
        CHECK(reader.damage()[0] != '\0');
        CHECK(reader.damage_offset() == 0u);
        return 0u;
    }
    std::uint32_t       count = 0u;
    RecordHeader        record;
    const std::uint8_t *payload = nullptr;
    for (;;) {
        const Status status = reader.next(record, payload);
        if (status == Status::not_found) break;
        if (status == Status::malformed) {
            damaged = true;
            CHECK(reader.damaged());
            CHECK(reader.damage()[0] != '\0');
            CHECK(reader.damage_offset() < length);
            CHECK_STATUS(reader.next(record, payload), Status::malformed);
            CHECK(payload == nullptr);
            break;
        }
        CHECK_STATUS(status, Status::ok);
        CHECK(payload != nullptr);
        CHECK(payload + record.length() <= bytes + length);
        count++;
        CHECK(reader.invariants_hold());
    }
    CHECK_EQ(reader.records(), count);
    if (damaged) CHECK(reader.damage_offset() == reader.offset());
    else CHECK(reader.offset() == length);
    CHECK(reader.invariants_hold());
    return count;
}

}

namespace av {

struct RecordingReaderProbe {
    static void walk_past_the_end(RecordingReader &reader) { reader.offset_ = reader.length_ + 1u; }
    static void damage_past_the_end(RecordingReader &reader)
    {
        reader.damage_offset_ = reader.length_ + 1u;
    }
    static void answer_unopened(RecordingReader &reader)
    {
        reader.opened_ = false;
        reader.offset_ = 0u;
        reader.records_ = 1u;
    }
    static void walk_unopened(RecordingReader &reader) { reader.opened_ = false; }
    static void stand_in_the_header(RecordingReader &reader) { reader.offset_ = 0u; }
    static void break_the_header(RecordingReader &reader)
    {
        reader.header_ = RecordingHeader(ProtocolVersion(1u, 1u), 0u, 0u,
                                         RecordingHeader::encoded_size - 1u, 0u, 0u, nullptr);
    }
};

}

TEST(recording_reader_open, reads_the_file_header_the_device_wrote)
{ const Written written;
  RecordingReader reader(written.bytes(), written.length());
  CHECK(!reader.opened());
  CHECK(reader.invariants_hold());
  CHECK_STATUS(reader.open(), Status::ok);
  CHECK(reader.opened());
  CHECK_EQ(reader.header().vendor_id(), 0x1002u);
  CHECK_EQ(reader.header().device_id(), 0x73DFu);
  CHECK_EQ(reader.header().wire_version().minor_version(), 1u);
  CHECK(reader.offset() == RecordingHeader::encoded_size);
  CHECK_STATUS(reader.open(), Status::invalid_state);
  CHECK(!reader.damaged());
  CHECK(reader.damage()[0] == '\0'); }

TEST(recording_reader_next, answers_every_record_with_its_payload_then_ends)
{ const Written written;
  RecordingReader reader(written.bytes(), written.length());
  CHECK_STATUS(reader.open(), Status::ok);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;

  CHECK_STATUS(reader.next(record, payload), Status::ok);
  CHECK(record.kind() == RecordKind::batch);
  CHECK_EQ(record.context(), 4u);
  CHECK(record.fence() == 1u);
  CHECK_EQ(record.length(), 5u);
  CHECK_EQ(payload[0], 0xA0u);
  CHECK_EQ(payload[4], 0xA4u);

  CHECK_STATUS(reader.next(record, payload), Status::ok);
  CHECK(record.kind() == RecordKind::reply);
  CHECK_EQ(payload[1], 0xB1u);

  CHECK_STATUS(reader.next(record, payload), Status::ok);
  CHECK(record.kind() == RecordKind::fence_completed);
  CHECK_EQ(record.length(), 0u);

  CHECK_STATUS(reader.next(record, payload), Status::ok);
  CHECK(record.kind() == RecordKind::fault);
  CHECK(record.fence() == 2u);
  CHECK_EQ(payload[0], static_cast<std::uint8_t>(FaultCode::device_lost));

  CHECK_STATUS(reader.next(record, payload), Status::ok);
  CHECK(record.kind() == RecordKind::blob);
  CHECK_EQ(record.context(), 0u);

  CHECK_STATUS(reader.next(record, payload), Status::not_found);
  CHECK(payload == nullptr);
  CHECK_STATUS(reader.next(record, payload), Status::not_found);
  CHECK_EQ(reader.records(), 5u);
  CHECK(!reader.damaged());
  CHECK(reader.offset() == written.length()); }

TEST(recording_reader_next, refuses_to_read_before_the_recording_is_opened)
{ const Written written;
  RecordingReader reader(written.bytes(), written.length());
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  CHECK_STATUS(reader.next(record, payload), Status::invalid_state);
  CHECK(payload == nullptr);
  CHECK(!reader.damaged());
  CHECK_EQ(reader.records(), 0u); }

TEST(recording_reader_open, an_empty_or_short_file_is_damage_at_offset_zero)
{ const Written written;
  for (std::size_t length = 0u; length < RecordingHeader::encoded_size; length++) {
      RecordingReader reader(written.bytes(), length);
      CHECK_STATUS(reader.open(), Status::malformed);
      CHECK(reader.damaged());
      CHECK(reader.damage_offset() == 0u);
      CHECK(!reader.opened());
      CHECK(reader.invariants_hold());
  }
  RecordingReader none(nullptr, 1024u);
  CHECK_STATUS(none.open(), Status::malformed);
  CHECK(none.damaged()); }

TEST(recording_reader_open, a_recording_from_a_later_version_says_so_rather_than_calling_it_damage)
{ Written        written;
  std::uint8_t   bytes[widest] = {};
  std::memcpy(bytes, written.bytes(), written.length());
  bytes[4] = RecordingHeader::current_major + 1u;
  RecordingReader reader(bytes, written.length());
  CHECK_STATUS(reader.open(), Status::not_supported);
  CHECK(reader.damaged());
  CHECK(std::strstr(reader.damage(), "later version") != nullptr); }

TEST(recording_reader_next, a_recording_cut_at_any_offset_answers_what_was_whole)
{ const Written       written;
  const std::uint32_t payloads[] = {5u, 2u, 0u, 8u, WireRecorder::blob_payload};
  std::size_t         ends[5] = {};
  std::size_t         at = RecordingHeader::encoded_size;
  for (std::size_t which = 0u; which < 5u; which++) {
      at += RecordHeader::encoded_size + payloads[which];
      ends[which] = at;
  }
  CHECK_EQ(at, written.length());
  for (std::size_t length = RecordingHeader::encoded_size; length <= written.length(); length++) {
      std::uint32_t whole = 0u;
      bool          clean = length == RecordingHeader::encoded_size;
      for (const std::size_t end : ends) {
          whole += end <= length ? 1u : 0u;
          clean = clean || end == length;
      }
      bool                damaged = false;
      const std::uint32_t records = walked(written.bytes(), length, damaged);
      CHECK_EQ(records, whole);
      CHECK_EQ(damaged, !clean);
  } }

TEST(recording_reader_next, a_record_length_past_the_end_is_damage_where_that_record_starts)
{ Written      written;
  std::uint8_t bytes[widest] = {};
  std::memcpy(bytes, written.bytes(), written.length());
  const std::size_t first = RecordingHeader::encoded_size;
  bytes[first + 12u] = 0xFFu;
  bytes[first + 13u] = 0xFFu;
  RecordingReader reader(bytes, written.length());
  CHECK_STATUS(reader.open(), Status::ok);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  CHECK_STATUS(reader.next(record, payload), Status::malformed);
  CHECK(reader.damage_offset() == first);
  CHECK(std::strstr(reader.damage(), "passes the end") != nullptr);
  CHECK_EQ(reader.records(), 0u);
  CHECK(reader.invariants_hold()); }

TEST(recording_reader_next, a_kind_no_device_wrote_is_damage_where_that_record_starts)
{ Written      written;
  std::uint8_t bytes[widest] = {};
  std::memcpy(bytes, written.bytes(), written.length());
  const std::size_t second = RecordingHeader::encoded_size + RecordHeader::encoded_size + 5u;
  bytes[second + 4u] = 9u;
  RecordingReader reader(bytes, written.length());
  CHECK_STATUS(reader.open(), Status::ok);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  CHECK_STATUS(reader.next(record, payload), Status::ok);
  CHECK_STATUS(reader.next(record, payload), Status::malformed);
  CHECK(reader.damage_offset() == second);
  CHECK(std::strstr(reader.damage(), "no device wrote") != nullptr);
  CHECK_EQ(reader.records(), 1u); }

TEST(recording_reader_next, survives_a_byte_flip_anywhere_in_a_recording)
{ const Written       written;
  const std::uint32_t payloads[] = {5u, 2u, 0u, 8u, WireRecorder::blob_payload};
  std::size_t         starts[5] = {};
  std::size_t         at = RecordingHeader::encoded_size;
  for (std::size_t which = 0u; which < 5u; which++) {
      starts[which] = at;
      at += RecordHeader::encoded_size + payloads[which];
  }
  CHECK_EQ(at, written.length());
  for (std::size_t index = 0u; index < written.length(); index++) {
      for (std::uint8_t bit = 0u; bit < 8u; bit++) {
          std::uint8_t bytes[widest] = {};
          std::memcpy(bytes, written.bytes(), written.length());
          bytes[index] = static_cast<std::uint8_t>(bytes[index] ^ (1u << bit));
          bool                damaged = false;
          const std::uint32_t records = walked(bytes, written.length(), damaged);
          CHECK(records <= 5u);
          if (index < 4u || (index >= 44u && index < 48u)) {
              CHECK(damaged);
              CHECK_EQ(records, 0u);
              continue;
          }
          for (std::size_t which = 0u; which < 5u; which++) {
              const bool magic = index >= starts[which] && index < starts[which] + 4u;
              const bool reserved = index == starts[which] + 6u || index == starts[which] + 7u;
              if (!magic && !reserved) continue;
              CHECK(damaged);
              CHECK_EQ(records, static_cast<std::uint32_t>(which));
          }
          for (std::size_t which = 0u; which < 5u; which++) {
              const std::size_t payload = starts[which] + RecordHeader::encoded_size;
              if (index < payload || index >= payload + payloads[which]) continue;
              CHECK(!damaged);
              CHECK_EQ(records, 5u);
          }
      }
  } }

TEST(recording_reader_invariants, catch_a_walk_or_a_damage_outside_the_bytes_it_was_given)
{ const Written written;
  RecordingReader reader(written.bytes(), written.length());
  CHECK_STATUS(reader.open(), Status::ok);
  CHECK(reader.invariants_hold());
  av::RecordingReaderProbe::walk_past_the_end(reader);
  CHECK(!reader.invariants_hold());

  RecordingReader damaged(written.bytes(), written.length());
  CHECK_STATUS(damaged.open(), Status::ok);
  av::RecordingReaderProbe::damage_past_the_end(damaged);
  CHECK(!damaged.invariants_hold());

  RecordingReader unopened(written.bytes(), written.length());
  CHECK_STATUS(unopened.open(), Status::ok);
  av::RecordingReaderProbe::answer_unopened(unopened);
  CHECK(!unopened.invariants_hold());

  RecordingReader walking(written.bytes(), written.length());
  CHECK_STATUS(walking.open(), Status::ok);
  av::RecordingReaderProbe::walk_unopened(walking);
  CHECK(!walking.invariants_hold());

  RecordingReader standing(written.bytes(), written.length());
  CHECK_STATUS(standing.open(), Status::ok);
  av::RecordingReaderProbe::stand_in_the_header(standing);
  CHECK(!standing.invariants_hold());

  RecordingReader broken(written.bytes(), written.length());
  CHECK_STATUS(broken.open(), Status::ok);
  av::RecordingReaderProbe::break_the_header(broken);
  CHECK(!broken.invariants_hold()); }
