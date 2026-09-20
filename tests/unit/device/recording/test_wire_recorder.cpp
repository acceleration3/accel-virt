#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_header.hpp"
#include "device/recording/recording_limit.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gl_context_options.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/fault_code.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <pthread.h>

using av::AtCeiling;
using av::BlobAction;
using av::ContextSetup;
using av::FaultCode;
using av::GlContextOptions;
using av::GpuIdentity;
using av::MockHost;
using av::ProtocolVersion;
using av::RecordHeader;
using av::RecordingHeader;
using av::RecordingLimit;
using av::RecordedFile;
using av::RecordKind;
using av::Status;
using av::SubApi;
using av::WireRecorder;

namespace {

constexpr const char *path = "run.rvk";

const std::uint8_t uuid[GpuIdentity::uuid_bytes] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u,
                                                    9u, 10u, 11u, 12u, 13u, 14u, 15u, 16u};

GpuIdentity a_gpu()
{
    return GpuIdentity(0x10DEu, 0x2204u, GpuIdentity::discrete_gpu, uuid, 1u << 30, 0u, "mock gpu");
}

ContextSetup a_setup()
{
    return ContextSetup(ProtocolVersion(1u, 0u), SubApi::vulkan, 40u, 16u, 41u, 2u, 42u);
}

struct Racer {
    WireRecorder *recorder;
    std::uint32_t context;
};

void *record_many(void *argument)
{
    Racer &racer = *static_cast<Racer *>(argument);
    std::uint8_t payload[64];
    std::memset(payload, static_cast<int>(racer.context), sizeof payload);
    for (std::uint64_t fence = 1u; fence <= 64u; fence++)
        racer.recorder->record_batch(racer.context, fence, payload, sizeof payload);
    return nullptr;
}

}  // namespace

namespace av {

struct WireRecorderProbe {
    /* A recorder holding a file with no lock, or a lock with no file: neither
     * survives open() or close(). */
    static void lose_the_lock(WireRecorder &recorder) { recorder.lock_ = nullptr; }
    static void restore_the_lock(WireRecorder &recorder, void *lock) { recorder.lock_ = lock; }
    static void *lock_of(WireRecorder &recorder) { return recorder.lock_; }
    static void break_the_header(WireRecorder &recorder)
    {
        recorder.header_ = RecordingHeader(ProtocolVersion(1u, 1u), 0u, 0u,
                                           RecordingHeader::encoded_size - 1u, 0u, 0u, nullptr);
    }
    static void break_the_limit(WireRecorder &recorder) { recorder.limit_.took(recorder.limit_.ceiling()); }
};

}  // namespace av

TEST(wire_recorder, without_a_path_it_records_nothing_and_asks_the_host_for_nothing)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(nullptr, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(!recorder.recording());
  CHECK(recorder.invariants_hold());
  recorder.record_batch(1u, 1u, uuid, sizeof uuid);
  recorder.record_fence(1u, 1u);
  recorder.record_fault(1u, 1u, FaultCode::undecodable);
  recorder.record_reply(1u, 1u, uuid, sizeof uuid);
  recorder.record_context_setup(1u, a_setup());
  recorder.record_blob(1u, BlobAction::created, 0u, 1u, 0u, 4096u);
  recorder.record_blob_bytes(1u, 1u, 0u, uuid, sizeof uuid);
  CHECK(recorder.records() == 0u);
  CHECK_EQ(host.live_mutexes(), 0u);
  CHECK_EQ(host.recording_length(path), 0u);
  CHECK_STATUS(recorder.open("", 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(!recorder.recording()); }

TEST(wire_recorder_open, writes_the_file_header_the_reader_will_find)
{ MockHost host; WireRecorder recorder(host);
  host.advance_clock_per_read(1000u);
  CHECK_STATUS(recorder.open(path, 1u << 20, "all", AtCeiling::wrap, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(recorder.recording());
  CHECK(recorder.invariants_hold());
  CHECK_EQ(host.recording_length(path), RecordingHeader::encoded_size);
  RecordedFile file(host, path);
  CHECK_EQ(file.header().kinds(), 0x7Fu);
  CHECK(file.header().limit() == (1u << 20));
  CHECK_EQ(file.header().vendor_id(), 0x10DEu);
  CHECK_EQ(file.header().device_id(), 0x2204u);
  CHECK_EQ(file.header().wire_version().minor_version(), 1u);
  CHECK(std::memcmp(file.header().uuid(), uuid, sizeof uuid) == 0);
  recorder.close();
  CHECK(!recorder.recording());
  CHECK(recorder.invariants_hold()); }

TEST(wire_recorder, writes_one_record_of_each_kind_with_its_payload)
{ MockHost host; WireRecorder recorder(host);
  host.advance_clock_per_read(1000u);
  CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)), Status::ok);
  const std::uint8_t batch[] = {0xAAu, 0xBBu, 0xCCu, 0xDDu};
  const std::uint8_t answer[] = {0x11u, 0x22u};
  const std::uint8_t snapshot[] = {0xEEu, 0xEFu, 0xF0u};
  recorder.record_context_setup(3u, a_setup());
  recorder.record_batch(3u, 7u, batch, sizeof batch);
  recorder.record_reply(3u, 7u, answer, sizeof answer);
  recorder.record_fence(3u, 7u);
  recorder.record_fault(3u, 7u, FaultCode::undecodable);
  recorder.record_blob(3u, BlobAction::imported, 5u, 9u, 4096u, 8192u);
  recorder.record_blob_bytes(3u, 9u, 4096u, snapshot, sizeof snapshot);
  CHECK(recorder.records() == 7u);
  CHECK(recorder.refused() == 0u);
  CHECK(recorder.invariants_hold());

  RecordedFile        file(host, path);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  std::uint64_t       previous = 0u;

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::context_setup);
  CHECK_EQ(record.context(), 3u);
  CHECK(record.fence() == 0u);
  CHECK_EQ(record.length(), ContextSetup::base_size);
  { ContextSetup decoded;
    CHECK_STATUS(decoded.decode(payload, record.length()), Status::ok);
    CHECK_EQ(decoded.ring_resource(), 40u);
    CHECK_EQ(decoded.doorbell(), 2u); }
  CHECK(record.timestamp() >= previous);
  previous = record.timestamp();

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::batch);
  CHECK(record.fence() == 7u);
  CHECK_EQ(record.length(), sizeof batch);
  CHECK(std::memcmp(payload, batch, sizeof batch) == 0);
  CHECK(record.timestamp() > previous);
  previous = record.timestamp();

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::reply);
  CHECK(record.fence() == 7u);
  CHECK(std::memcmp(payload, answer, sizeof answer) == 0);

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::fence_completed);
  CHECK_EQ(record.length(), 0u);
  CHECK(record.fence() == 7u);

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::fault);
  CHECK_EQ(record.length(), 8u);
  CHECK_EQ(payload[0], static_cast<std::uint8_t>(FaultCode::undecodable));

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::blob);
  CHECK_EQ(record.length(), WireRecorder::blob_payload);
  CHECK_EQ(payload[0], static_cast<std::uint8_t>(BlobAction::imported));
  CHECK_EQ(payload[4], 5u);
  CHECK_EQ(payload[8], 9u);

  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::blob_bytes);
  CHECK_EQ(record.length(), WireRecorder::blob_bytes_prefix + sizeof snapshot);
  CHECK_EQ(payload[0], 9u);
  CHECK(std::memcmp(payload + WireRecorder::blob_bytes_prefix, snapshot, sizeof snapshot) == 0);

  CHECK(!file.next(record, payload));
  recorder.close(); }

TEST(wire_recorder, keeps_only_the_kinds_it_was_asked_for)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, "batch,reply", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u};
  recorder.record_context_setup(3u, a_setup());
  recorder.record_batch(3u, 1u, batch, sizeof batch);
  recorder.record_fence(3u, 1u);
  recorder.record_fault(3u, 1u, FaultCode::device_lost);
  recorder.record_blob(3u, BlobAction::released, 0u, 1u, 0u, 0u);
  recorder.record_blob_bytes(3u, 1u, 0u, batch, sizeof batch);
  recorder.record_reply(3u, 1u, batch, sizeof batch);
  CHECK(recorder.records() == 2u);
  RecordedFile        file(host, path);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::batch);
  CHECK(file.next(record, payload));
  CHECK(record.kind() == RecordKind::reply);
  CHECK(!file.next(record, payload));
  recorder.close(); }

TEST(wire_recorder_open, refuses_kinds_that_name_nothing_and_a_ceiling_below_the_header)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, "batch,nonsense", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::invalid_parameter);
  CHECK(!recorder.recording());
  CHECK_STATUS(recorder.open(path, RecordingHeader::encoded_size - 1u, nullptr, AtCeiling::stop, a_gpu(),
                             ProtocolVersion(1u, 1u)),
               Status::invalid_parameter);
  CHECK(!recorder.recording());
  CHECK_EQ(host.recording_length(path), 0u);
  CHECK(recorder.invariants_hold()); }

TEST(wire_recorder_open, refuses_a_second_recording_on_one_device)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK_STATUS(recorder.open("other.rvk", 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::invalid_state);
  CHECK(recorder.recording());
  CHECK_EQ(host.recording_length("other.rvk"), 0u);
  recorder.close(); }

TEST(wire_recorder_open, a_host_that_cannot_make_the_file_leaves_recording_off_and_says_so)
{ MockHost host; WireRecorder recorder(host);
  host.fail_recording_create_at(1u);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  CHECK_EQ(host.live_mutexes(), 0u);
  CHECK(recorder.invariants_hold()); }

TEST(wire_recorder_open, a_host_with_no_mutex_leaves_recording_off_and_says_so)
{ MockHost host; WireRecorder recorder(host);
  host.fail_mutex_at(1u);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  CHECK_EQ(host.live_recordings(), 0u); }

TEST(wire_recorder_open, a_file_header_that_cannot_be_written_stops_the_recording_at_once)
{ MockHost host; WireRecorder recorder(host);
  host.fail_recording_append_at(1u);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  const std::uint8_t batch[] = {1u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  CHECK(recorder.records() == 0u);
  CHECK_EQ(host.recording_length(path), 0u);
  recorder.close(); }

TEST(wire_recorder, a_write_the_host_refuses_stops_the_recording_and_logs_once)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  CHECK(recorder.records() == 1u);
  host.fail_recording_append_at(1u);
  recorder.record_batch(1u, 2u, batch, sizeof batch);
  recorder.record_batch(1u, 3u, batch, sizeof batch);
  CHECK(recorder.records() == 1u);
  /* A write the host refused is a record lost, not a record turned away: the
   * count is what the ceiling and a record's own size raise. */
  CHECK_EQ(recorder.refused(), 0u);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  CHECK_EQ(host.recording_length(path),
           RecordingHeader::encoded_size + RecordHeader::encoded_size + sizeof batch);
  recorder.close(); }

TEST(wire_recorder, a_payload_that_carries_a_record_but_not_its_bulk_stops_the_recording)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  /* The header lands, the bulk does not. */
  host.fail_recording_append_at(2u);
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  CHECK(recorder.records() == 0u);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  recorder.close(); }

TEST(wire_recorder, the_ceiling_stops_the_recording_and_says_so_once)
{ MockHost host; WireRecorder recorder(host);
  const std::uint64_t ceiling = RecordingHeader::encoded_size + 2u * (RecordHeader::encoded_size + 4u);
  CHECK_STATUS(recorder.open(path, ceiling, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  for (std::uint64_t fence = 1u; fence <= 5u; fence++) recorder.record_batch(1u, fence, batch, sizeof batch);
  CHECK(recorder.records() == 2u);
  CHECK(recorder.refused() == 1u);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  CHECK(host.recording_length(path) == ceiling);
  CHECK_EQ(RecordedFile(host, path).remaining(), 2u);
  recorder.close(); }

TEST(wire_recorder, a_wrapping_recording_starts_again_at_its_header_and_never_passes_the_ceiling)
{ MockHost host; WireRecorder recorder(host);
  const std::uint64_t ceiling = RecordingHeader::encoded_size + 2u * (RecordHeader::encoded_size + 4u);
  CHECK_STATUS(recorder.open(path, ceiling, nullptr, AtCeiling::wrap, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  for (std::uint64_t fence = 1u; fence <= 5u; fence++) {
      recorder.record_batch(1u, fence, batch, sizeof batch);
      CHECK(host.recording_length(path) <= ceiling);
      CHECK(recorder.invariants_hold());
  }
  CHECK(recorder.records() == 5u);
  CHECK(recorder.recording());
  CHECK_EQ(recorder.wraps(), 2u);
  RecordedFile        file(host, path);
  CHECK_EQ(file.header().wraps(), 2u);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  CHECK(file.next(record, payload));
  /* The newest records are what survived. */
  CHECK(record.fence() == 5u);
  CHECK(!file.next(record, payload));
  recorder.close(); }

TEST(wire_recorder, a_wrap_the_host_refuses_stops_the_recording)
{ MockHost host; WireRecorder recorder(host);
  const std::uint64_t ceiling = RecordingHeader::encoded_size + RecordHeader::encoded_size + 4u;
  CHECK_STATUS(recorder.open(path, ceiling, nullptr, AtCeiling::wrap, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  host.fail_recording_truncate_at(1u);
  recorder.record_batch(1u, 2u, batch, sizeof batch);
  CHECK(recorder.records() == 1u);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  recorder.close(); }

TEST(wire_recorder, a_wrap_whose_header_cannot_be_rewritten_stops_the_recording)
{ MockHost host; WireRecorder recorder(host);
  const std::uint64_t ceiling = RecordingHeader::encoded_size + RecordHeader::encoded_size + 4u;
  CHECK_STATUS(recorder.open(path, ceiling, nullptr, AtCeiling::wrap, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  /* The truncate lands; the header rewrite does not. */
  host.fail_recording_append_at(1u);
  recorder.record_batch(1u, 2u, batch, sizeof batch);
  CHECK(recorder.records() == 1u);
  CHECK(!recorder.recording());
  CHECK_EQ(host.logged(), 1u);
  recorder.close(); }

TEST(wire_recorder, a_record_larger_than_any_record_may_be_is_refused_and_the_recording_runs_on)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)), Status::ok);
  const std::uint8_t byte = 1u;
  /* The first size a record may not hold: its prefix plus this is one byte
   * past max_payload. The accepting side, a record of exactly max_payload,
   * cannot be written here - MockHost's recording holds 4 MiB. */
  recorder.record_blob_bytes(1u, 9u, 0u, &byte, RecordHeader::max_payload - WireRecorder::blob_bytes_prefix + 1u);
  CHECK(recorder.records() == 0u);
  CHECK(recorder.refused() == 1u);
  recorder.record_blob_bytes(1u, 9u, 0u, &byte, RecordHeader::max_payload);
  CHECK(recorder.records() == 0u);
  CHECK(recorder.refused() == 2u);
  CHECK(recorder.recording());
  const std::uint8_t batch[] = {1u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  CHECK(recorder.records() == 1u);
  recorder.close(); }

TEST(wire_recorder, a_setup_that_cannot_be_encoded_is_refused_and_the_recording_runs_on)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)), Status::ok);
  /* Minor 0 carries no OpenGL options, so this setup has no encoding. */
  const ContextSetup impossible(ProtocolVersion(1u, 0u), SubApi::opengl, 40u, 16u, 41u, 2u, 42u,
                                GlContextOptions(4u, 6u, GlContextOptions::Profile::core, 0u, 0u));
  recorder.record_context_setup(3u, impossible);
  CHECK(recorder.records() == 0u);
  CHECK(recorder.refused() == 1u);
  CHECK(recorder.recording());
  recorder.close(); }

TEST(wire_recorder, several_workers_recording_at_once_leave_whole_records_in_order)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  Racer     racers[4] = {{&recorder, 1u}, {&recorder, 2u}, {&recorder, 3u}, {&recorder, 4u}};
  pthread_t threads[4] = {};
  for (std::size_t index = 0u; index < 4u; index++)
      CHECK_EQ(pthread_create(&threads[index], nullptr, record_many, &racers[index]), 0);
  for (pthread_t thread : threads) CHECK_EQ(pthread_join(thread, nullptr), 0);
  CHECK(recorder.records() == 4u * 64u);
  CHECK(recorder.refused() == 0u);

  RecordedFile        file(host, path);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  std::uint32_t       seen = 0u;
  std::uint64_t       last = 0u;
  while (file.next(record, payload)) {
      seen++;
      CHECK_EQ(record.length(), 64u);
      /* Every byte of a record belongs to the context that wrote it. */
      for (std::uint32_t index = 0u; index < record.length(); index++)
          CHECK_EQ(payload[index], static_cast<std::uint8_t>(record.context()));
      CHECK(record.timestamp() >= last);
      last = record.timestamp();
  }
  CHECK_EQ(seen, 4u * 64u);
  recorder.close(); }

/* Constitution IV: with each recording call failed in turn, the device runs on,
 * the recorder stops cleanly, nothing leaks and no recording is left open -
 * and each refusal leaves a different, named state, decided from the contract
 * rather than counted. The ceiling holds the file header and one four-byte
 * record, so the second batch is the one that wraps. */
TEST(wire_recorder, survives_every_recording_call_the_host_refuses_in_turn)
{ struct Refusal {
      std::uint32_t nth;
      std::uint64_t records;   /* what reached the file before the refusal */
      std::size_t   written;   /* what the file holds afterwards */
      const char   *what;
  };
  const std::uint64_t one_record = RecordHeader::encoded_size + 4u;
  const Refusal       refusals[] = {
      /* The file is never made: recording is off, nothing is written, and the
       * run is not refused. */
      {1u, 0u, 0u, "create"},
      /* The file header lands, the first batch's frame does not. */
      {2u, 0u, RecordingHeader::encoded_size, "the first record"},
      /* The first batch lands; the wrap the second one needs cannot be made. */
      {3u, 1u, RecordingHeader::encoded_size + one_record, "the wrap"},
      /* No mutex, so no record is ever taken, and the header was never written
       * either: open gave up on its first write. */
      {4u, 0u, 0u, "the mutex"}};
  for (const Refusal &refusal : refusals) {
      MockHost host; WireRecorder recorder(host);
      host.fail_recording_create_at(refusal.nth == 1u ? 1u : 0u);
      host.fail_recording_append_at(refusal.nth == 2u ? 2u : 0u);
      host.fail_recording_truncate_at(refusal.nth == 3u ? 1u : 0u);
      host.fail_mutex_at(refusal.nth == 4u ? 1u : 0u);
      const std::uint64_t ceiling = RecordingHeader::encoded_size + one_record;
      CHECK_STATUS(recorder.open(path, ceiling, "batch", AtCeiling::wrap, a_gpu(), ProtocolVersion(1u, 1u)),
                   Status::ok);
      const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
      for (std::uint64_t fence = 1u; fence <= 4u; fence++) {
          recorder.record_batch(7u, fence, batch, sizeof batch);
          CHECK(recorder.invariants_hold());
      }
      /* One line about what was refused, and never a second one per record. */
      CHECK_EQ(host.logged(), 1u);
      CHECK(!recorder.recording());
      CHECK_EQ(recorder.records(), refusal.records);
      CHECK_EQ(recorder.refused(), 0u);
      CHECK_EQ(host.recording_length(path), refusal.written);
      recorder.close();
      CHECK(recorder.invariants_hold());
      CHECK_EQ(host.live_recordings(), 0u);
      CHECK_EQ(host.live_mutexes(), 0u);
  } }

TEST(wire_recorder, keeps_no_batch_and_no_reply_when_it_was_not_asked_for_them)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, "fence", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t bytes[] = {1u, 2u};
  recorder.record_batch(1u, 1u, bytes, sizeof bytes);
  recorder.record_reply(1u, 1u, bytes, sizeof bytes);
  CHECK(recorder.records() == 0u);
  recorder.record_fence(1u, 1u);
  CHECK(recorder.records() == 1u);
  recorder.close(); }

TEST(wire_recorder, a_record_taken_after_the_recording_stopped_writes_nothing)
{ MockHost host; WireRecorder recorder(host);
  const std::uint64_t ceiling = RecordingHeader::encoded_size + RecordHeader::encoded_size + 4u;
  CHECK_STATUS(recorder.open(path, ceiling, nullptr, AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  recorder.record_batch(1u, 2u, batch, sizeof batch);   /* stops the recording */
  CHECK(!recorder.recording());
  const std::uint64_t written = host.recording_length(path);
  /* The kind is still kept, so the call reaches write, which sees the
   * recording stopped under the lock and leaves the file alone. */
  recorder.record_batch(1u, 3u, batch, sizeof batch);
  CHECK(recorder.records() == 1u);
  CHECK_EQ(host.logged(), 1u);
  CHECK(host.recording_length(path) == written);
  recorder.close(); }

TEST(wire_recorder_invariants, catch_a_recording_whose_parts_do_not_agree)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 1u << 20, "all", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  CHECK(recorder.invariants_hold());
  void *const lock = av::WireRecorderProbe::lock_of(recorder);
  av::WireRecorderProbe::lose_the_lock(recorder);
  CHECK(!recorder.invariants_hold());
  av::WireRecorderProbe::restore_the_lock(recorder, lock);
  av::WireRecorderProbe::break_the_header(recorder);
  CHECK(!recorder.invariants_hold());
  recorder.close();

  MockHost     other; WireRecorder second(other);
  CHECK_STATUS(second.open(path, 1u << 20, "all", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)),
               Status::ok);
  av::WireRecorderProbe::break_the_limit(second);
  CHECK(!second.invariants_hold());
  second.close(); }

TEST(wire_recorder_invariants, catch_a_closed_recorder_still_holding_a_lock)
{ MockHost host; WireRecorder recorder(host);
  CHECK(recorder.invariants_hold());
  av::WireRecorderProbe::restore_the_lock(recorder, &host);
  CHECK(!recorder.invariants_hold());
  av::WireRecorderProbe::restore_the_lock(recorder, nullptr);
  CHECK(recorder.invariants_hold()); }

/* The counts are what a run reads once the recording is closed, so closing a
 * recorder that recorded is the ordinary end of one, not a broken recorder. */
TEST(wire_recorder, a_closed_recording_keeps_its_counts_and_holds_its_invariant)
{ MockHost host; WireRecorder recorder(host);
  CHECK_STATUS(recorder.open(path, 0u, "all", AtCeiling::stop, a_gpu(), ProtocolVersion(1u, 1u)), Status::ok);
  const std::uint8_t batch[] = {1u, 2u, 3u, 4u};
  recorder.record_batch(1u, 1u, batch, sizeof batch);
  recorder.record_blob_bytes(1u, 9u, 0u, batch, RecordHeader::max_payload);
  CHECK_EQ(recorder.records(), 1u);
  CHECK_EQ(recorder.refused(), 1u);
  recorder.close();
  CHECK(recorder.invariants_hold());
  CHECK_EQ(recorder.records(), 1u);
  CHECK_EQ(recorder.refused(), 1u);
  CHECK(!recorder.recording()); }
