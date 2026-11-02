#include "miniport/av_context/guest_context.hpp"
#include "core/support/status.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/recording_header.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/loopback/loopback_rig.hpp"
#include "device/execution/executor_library.hpp"
#include "device/replay/recording_filter.hpp"
#include "device/replay/recording_reader.hpp"
#include "device/replay/recording_replay.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/reply_header.hpp"

#include <cstdint>
#include <cstring>

using av::CommandHeader;
using av::GuestRing;
using av::LoopbackRig;
using av::RecordedFile;
using av::RecordHeader;
using av::RecordKind;
using av::Status;
using av::SubApi;

namespace {

constexpr const char *path = "loopback.av";

class EchoBatch {
public:
    explicit EchoBatch(const char *text) : bytes_{}, length_(CommandHeader::encoded_size + 8u)
    {
        static_cast<void>(CommandHeader(GuestRing::filler_command, static_cast<std::uint32_t>(length_),
                                        CommandHeader::generate_reply)
                              .encode(bytes_, sizeof bytes_));
        std::memcpy(bytes_ + CommandHeader::encoded_size, text, 8u);
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::uint32_t       length() const { return static_cast<std::uint32_t>(length_); }

private:
    std::uint8_t bytes_[CommandHeader::encoded_size + 8u];
    std::size_t  length_;
};

}

TEST(loopback_recording, a_run_leaves_the_setup_the_batches_the_replies_and_the_fences)
{ LoopbackRig rig(av::LoopbackDevice::default_gpu, path);
  CHECK(rig.read_capset());
  CHECK(rig.establish(0u));
  GuestRing ring(rig.context(0u).ring(), LoopbackRig::ring_order);
  const EchoBatch batches[] = {EchoBatch("batch-01"), EchoBatch("batch-02"), EchoBatch("batch-03")};
  std::uint64_t   begin = 0u;
  for (std::uint64_t fence = 1u; fence <= 3u; fence++) {
      ring.write_batch(fence, batches[fence - 1u].bytes(), batches[fence - 1u].length());
      CHECK_STATUS(rig.submit(0u, fence, begin, ring.tail()), Status::ok);
      begin = ring.tail();
  }
  CHECK(rig.driver().serve_until(rig.context(0u), rig.line(0u), 3u));
  CHECK_EQ(rig.context(0u).completed(), 3u);

  RecordedFile        file(rig.device().host(), path);
  RecordHeader        record;
  const std::uint8_t *payload = nullptr;
  for (std::uint32_t blob = 0u; blob < 3u; blob++) {
      payload = file.expect(RecordKind::blob, record);
      CHECK(payload != nullptr);
      CHECK_EQ(record.context(), 0u);
      if (payload != nullptr) CHECK_EQ(payload[0], 0u);
  }
  payload = file.expect(RecordKind::context_setup, record);
  CHECK(payload != nullptr);
  if (payload != nullptr) {
      av::ContextSetup setup;
      CHECK_STATUS(setup.decode(payload, record.length()), Status::ok);
      CHECK(setup.sub_api() == SubApi::vulkan);
      CHECK_EQ(setup.ring_order(), LoopbackRig::ring_order);
      CHECK_EQ(setup.doorbell(), rig.line(0u));
  }
  const std::uint32_t context = record.context();
  CHECK(context != 0u);

  std::uint64_t taken = 0u;
  for (std::uint64_t fence = 1u; fence <= 3u; fence++) {
      payload = file.expect(RecordKind::batch, record);
      CHECK(payload != nullptr);
      CHECK(record.fence() == fence);
      CHECK_EQ(record.context(), context);
      CHECK_EQ(record.length(), batches[fence - 1u].length());
      if (payload != nullptr)
          CHECK_EQ(std::memcmp(payload, batches[fence - 1u].bytes(), record.length()), 0);
      CHECK(record.timestamp() >= taken);
      taken = record.timestamp();

      payload = file.expect(RecordKind::reply, record);
      CHECK(payload != nullptr);
      CHECK(record.fence() == fence);
      CHECK_EQ(record.length(), 8u);
      if (payload != nullptr) CHECK_EQ(std::memcmp(payload, batches[fence - 1u].bytes() + 8u, 8u), 0);

      payload = file.expect(RecordKind::fence_completed, record);
      CHECK(payload != nullptr);
      CHECK(record.fence() == fence);
      CHECK_EQ(record.length(), 0u);
  }
  CHECK_EQ(file.remaining(), 0u);
  CHECK(rig.retire(0u)); }

TEST(loopback_recording, a_batch_the_executor_could_not_decode_leaves_a_fault_record)
{ LoopbackRig rig(av::LoopbackDevice::default_gpu, path);
  CHECK(rig.read_capset());
  CHECK(rig.establish(0u));
  GuestRing    ring(rig.context(0u).ring(), LoopbackRig::ring_order);
  std::uint8_t payload[CommandHeader::encoded_size] = {};
  CHECK_STATUS(CommandHeader(av::MockExecutorOps::undecodable_id, sizeof payload, 0u)
                   .encode(payload, sizeof payload),
               Status::ok);
  ring.write_batch(1u, payload, sizeof payload);
  CHECK_STATUS(rig.submit(0u, 1u, 0u, ring.tail()), Status::ok);
  CHECK(!rig.driver().serve_until(rig.context(0u), rig.line(0u), 1u));
  CHECK(rig.context(0u).faulted());

  RecordedFile        file(rig.device().host(), path);
  RecordHeader        record;
  for (std::uint32_t blob = 0u; blob < 3u; blob++)
      CHECK(file.expect(RecordKind::blob, record) != nullptr);
  CHECK(file.expect(RecordKind::context_setup, record) != nullptr);
  CHECK(file.expect(RecordKind::batch, record) != nullptr);
  const std::uint8_t *fault = file.expect(RecordKind::fault, record);
  CHECK(fault != nullptr);
  CHECK(record.fence() == 1u);
  if (fault != nullptr) CHECK_EQ(fault[0], static_cast<std::uint8_t>(av::FaultCode::undecodable));
  CHECK_EQ(file.remaining(), 0u);
  CHECK(rig.retire(0u)); }

TEST(loopback_recording, a_recorded_run_replays_into_a_library_with_the_same_replies)
{ LoopbackRig rig(av::LoopbackDevice::default_gpu, path);
  CHECK(rig.read_capset());
  CHECK(rig.establish(0u));
  GuestRing       ring(rig.context(0u).ring(), LoopbackRig::ring_order);
  const EchoBatch batches[] = {EchoBatch("replay-1"), EchoBatch("replay-2")};
  std::uint64_t   begin = 0u;
  for (std::uint64_t fence = 1u; fence <= 2u; fence++) {
      ring.write_batch(fence, batches[fence - 1u].bytes(), batches[fence - 1u].length());
      CHECK_STATUS(rig.submit(0u, fence, begin, ring.tail()), Status::ok);
      begin = ring.tail();
  }
  CHECK(rig.driver().serve_until(rig.context(0u), rig.line(0u), 2u));

  av::MockExecutorOps  ops(AV_EXECUTOR_SUB_API_VULKAN);
  av::ExecutorLibrary  library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  av::RecordingReader  reader(rig.device().host().recording_bytes(path),
                              rig.device().host().recording_length(path));
  av::RecordingReplay  replay(library);
  const av::RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.contexts(), 1u);
  CHECK(replay.commands() == 2u);
  CHECK(replay.replies() == 2u);
  for (std::uint32_t index = 0u; index < replay.findings(); index++)
      av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "%s", replay.finding(index));
  CHECK_EQ(replay.findings(), 0u);
  CHECK(replay.invariants_hold());
  CHECK(rig.retire(0u)); }
