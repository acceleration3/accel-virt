#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "device/recording/recording_reader.hpp"
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

constexpr const char *path = "read.rvk";
constexpr std::size_t widest = 4096u;

/* A recording of one of each kind, as the device writes one. */
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

/* Walks `length` bytes to the end or to the damage: the records answered. */
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
            /* Once damaged it stays damaged and reads nothing more. */
            CHECK_STATUS(reader.next(record, payload), Status::malformed);
            CHECK(payload == nullptr);
            break;
        }
        CHECK_STATUS(status, Status::ok);
        CHECK(payload != nullptr);
        /* Every payload the reader answers is inside the bytes it was given. */
        CHECK(payload + record.length() <= bytes + length);
        count++;
        CHECK(reader.invariants_hold());
    }
    CHECK_EQ(reader.records(), count);
    /* Undamaged, the walk consumed the whole file; damaged, it stopped exactly
     * where the record it could not frame begins. */
    if (damaged) CHECK(reader.damage_offset() == reader.offset());
    else CHECK(reader.offset() == length);
    CHECK(reader.invariants_hold());
    return count;
}

}  // namespace

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

/* US2 scenario 3: cut anywhere, every whole record before the cut is answered
 * and the damage is reported with its offset. */
TEST(recording_reader_next, a_recording_cut_at_any_offset_answers_what_was_whole)
{ const Written written;
  std::uint32_t best = 0u;
  for (std::size_t length = RecordingHeader::encoded_size; length <= written.length(); length++) {
      bool                damaged = false;
      const std::uint32_t records = walked(written.bytes(), length, damaged);
      /* A longer file never answers fewer records than a shorter one. */
      CHECK(records >= best);
      best = records;
  }
  CHECK_EQ(best, 5u);
  bool                damaged = false;
  const std::uint32_t whole = walked(written.bytes(), written.length(), damaged);
  CHECK_EQ(whole, 5u);
  CHECK(!damaged); }

TEST(recording_reader_next, a_record_length_past_the_end_is_damage_where_that_record_starts)
{ Written      written;
  std::uint8_t bytes[widest] = {};
  std::memcpy(bytes, written.bytes(), written.length());
  /* The first record's length, at offset 12 of its header. */
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
  bytes[second + 4u] = 9u;   /* kind 9 */
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
{ const Written written;
  std::uint32_t damaged_cases = 0u;
  for (std::size_t index = 0u; index < written.length(); index++) {
      for (std::uint8_t bit = 0u; bit < 8u; bit++) {
          std::uint8_t bytes[widest] = {};
          std::memcpy(bytes, written.bytes(), written.length());
          bytes[index] = static_cast<std::uint8_t>(bytes[index] ^ (1u << bit));
          bool damaged = false;
          static_cast<void>(walked(bytes, written.length(), damaged));
          damaged_cases += damaged ? 1u : 0u;
      }
  }
  /* A flip in a payload byte changes nothing about the framing; a flip in a
   * magic, a kind, a length or the version is caught. */
  CHECK(damaged_cases > 32u); }
