#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/executor_library.hpp"
#include "device/recording/recorded_command.hpp"
#include "device/recording/recording_filter.hpp"
#include "device/recording/recording_limit.hpp"
#include "device/recording/recording_reader.hpp"
#include "device/recording/recording_replay.hpp"
#include "device/recording/replay_context.hpp"
#include "device/recording/wire_recorder.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/context_setup.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/command_header.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AtCeiling;
using av::BlobAction;
using av::CommandHeader;
using av::ContextSetup;
using av::ExecutorLibrary;
using av::GpuIdentity;
using av::MockExecutorOps;
using av::MockHost;
using av::ProtocolVersion;
using av::RecordingFilter;
using av::RecordingReader;
using av::RecordingReplay;
using av::ReplayContext;
using av::Status;
using av::SubApi;
using av::WireRecorder;

namespace {

constexpr const char  *path = "replay.rvk";
constexpr std::size_t  widest = 8192u;
constexpr std::uint8_t uuid[GpuIdentity::uuid_bytes] = {0x41u, 0x42u};

/* A batch payload of framed commands, and the answer the mock executor will
 * give for it: the mock echoes each reply-generating command's payload. */
class Batch {
public:
    Batch() : bytes_{}, length_(0u) {}

    Batch &add(std::uint32_t id, std::uint32_t payload_length, std::uint8_t flags = 0u,
               std::uint8_t fill = 0u)
    {
        const std::uint32_t size = CommandHeader::encoded_size + payload_length;
        CHECK_STATUS(CommandHeader(id, size, flags).encode(bytes_ + length_, sizeof bytes_ - length_),
                     Status::ok);
        for (std::uint32_t index = 0u; index < payload_length; index++)
            bytes_[length_ + CommandHeader::encoded_size + index] = fill;
        length_ += size;
        return *this;
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::uint32_t       length() const { return length_; }

private:
    std::uint8_t  bytes_[256];
    std::uint32_t length_;
};

/* A recording the device wrote, kept as bytes a replay can read. */
class Written {
public:
    Written() : host_(), recorder_(host_), length_(0u), bytes_{}
    {
        CHECK_STATUS(recorder_.open(path, 0u, "all", AtCeiling::stop,
                                    GpuIdentity(1u, 2u, GpuIdentity::discrete_gpu, uuid, 0u, 0u, "mock"),
                                    ProtocolVersion(1u, 1u)),
                     Status::ok);
    }

    WireRecorder &recorder() { return recorder_; }

    void set_up(std::uint32_t context, SubApi api)
    {
        recorder_.record_context_setup(
            context, ContextSetup(ProtocolVersion(1u, 0u), api, 40u, 16u, 41u, 1u, 42u));
    }

    /* A batch and the reply the mock executor would have produced for it; a
     * batch whose commands asked for no answer leaves no reply record, as the
     * device leaves none. */
    void ran(std::uint32_t context, std::uint64_t fence, const Batch &batch, const std::uint8_t *answer,
             std::uint32_t answer_length)
    {
        recorder_.record_batch(context, fence, batch.bytes(), batch.length());
        if (answer != nullptr) recorder_.record_reply(context, fence, answer, answer_length);
        recorder_.record_fence(context, fence);
    }

    void close()
    {
        recorder_.close();
        length_ = host_.recording_length(path);
        CHECK(length_ <= sizeof bytes_);
        if (length_ <= sizeof bytes_) std::memcpy(bytes_, host_.recording_bytes(path), length_);
    }

    std::uint8_t       *editable() { return bytes_; }
    const std::uint8_t *bytes() const { return bytes_; }
    std::size_t         length() const { return length_; }

private:
    MockHost     host_;
    WireRecorder recorder_;
    std::size_t  length_;
    std::uint8_t bytes_[widest];
};

/* A recording of one Vulkan context running one batch whose only command asks
 * for an answer; the mock echoes the payload, so `fill` is the answer. */
void one_batch(Written &written, std::uint8_t fill)
{
    written.set_up(3u, SubApi::vulkan);
    Batch batch;
    batch.add(0x00010006u, 4u, CommandHeader::generate_reply, fill);
    std::uint8_t answer[4] = {};
    std::memset(answer, fill, sizeof answer);
    written.ran(3u, 1u, batch, answer, sizeof answer);
    written.close();
}

}  // namespace

TEST(recording_replay_run, replays_a_recording_into_a_library_with_no_finding)
{ Written written;
  one_batch(written, 0x5Au);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.contexts(), 1u);
  CHECK(replay.commands() == 1u);
  CHECK(replay.replies() == 1u);
  CHECK_EQ(replay.findings(), 0u);
  CHECK_EQ(std::strcmp(replay.finding(0u), ""), 0);
  CHECK(replay.invariants_hold());
  /* The library was driven through the ABI the device drives. */
  CHECK_EQ(ops.created(), 1u);
  CHECK_EQ(ops.executed(), 1u);
  CHECK_EQ(ops.id_at(0u), 0x00010006u);
  CHECK_STATUS(replay.run(reader, filter), Status::invalid_state); }

TEST(recording_replay_run, destroys_every_executor_it_made)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  written.set_up(4u, SubApi::vulkan);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  { RecordingReader reader(written.bytes(), written.length());
    RecordingReplay replay(library);
    const RecordingFilter filter;
    CHECK_STATUS(replay.run(reader, filter), Status::ok);
    CHECK_EQ(replay.contexts(), 2u);
    CHECK_EQ(ops.live(), 2u); }
  CHECK_EQ(ops.live(), 0u);
  CHECK_EQ(ops.destroyed(), 2u);
  CHECK_EQ(ops.misuses(), 0u); }

TEST(recording_replay_run, names_the_fence_and_the_command_of_an_answer_that_differs)
{ Written written;
  one_batch(written, 0x5Au);
  /* The recorded reply is changed, as a host that answers differently would. */
  std::uint8_t *const bytes = written.editable();
  bool                changed = false;
  for (std::size_t index = 0u; index + 3u < written.length(); index++) {
      if (bytes[index] != 0x5Au || bytes[index + 1u] != 0x5Au || bytes[index + 2u] != 0x5Au
          || bytes[index + 3u] != 0x5Au)
          continue;
      /* The reply record's payload, which follows the batch's copy of it. */
      if (!changed) { changed = true; continue; }
      bytes[index + 1u] = 0x5Bu;
      break;
  }
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "ctx=3 fence=1 ") != nullptr);
  CHECK(std::strstr(replay.finding(0u), "replied 4 bytes, recorded 4") != nullptr);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_library_that_makes_no_executor_is_a_finding_and_not_a_crash)
{ Written written;
  one_batch(written, 0x11u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.fail_create_at(1u);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.contexts(), 0u);
  /* No executor, so the batch belongs to a context that was never set up. */
  CHECK_EQ(replay.findings(), 2u);
  CHECK(std::strstr(replay.finding(0u), "made no executor") != nullptr);
  CHECK(std::strstr(replay.finding(1u), "never set up") != nullptr);
  CHECK(replay.commands() == 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_command_the_library_cannot_decode_stops_that_context_where_it_did)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  Batch batch;
  batch.add(0x00010006u, 4u).add(MockExecutorOps::undecodable_id, 4u).add(0x00010007u, 4u);
  written.ran(3u, 7u, batch, nullptr, 0u);
  Batch later;
  later.add(0x00010008u, 4u);
  written.ran(3u, 8u, later, nullptr, 0u);
  written.close();

  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "ctx=3 fence=7 ") != nullptr);
  CHECK(std::strstr(replay.finding(0u), "could not decode command 2") != nullptr);
  /* The command that would have followed it never ran, and neither did the
   * batch after it: US3 scenario 4, stopping where the recording did. */
  CHECK_EQ(ops.executed(), 2u);
  CHECK(replay.commands() == 2u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_library_that_loses_its_device_stops_that_context)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  Batch batch;
  batch.add(MockExecutorOps::lost_id, 0u);
  written.ran(3u, 4u, batch, nullptr, 0u);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "lost its device at command 1") != nullptr);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_command_the_library_does_not_know_is_skipped_as_the_device_skips_it)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  Batch batch;
  batch.add(MockExecutorOps::unknown_id, 0u).add(0x00010006u, 4u);
  written.ran(3u, 2u, batch, nullptr, 0u);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 0u);
  CHECK(replay.commands() == 2u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_batch_that_imports_guest_memory_is_reported_unreplayable)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  Batch batch;
  batch.add(CommandHeader::import_blob_id, 32u).add(0x00010006u, 4u);
  written.ran(3u, 5u, batch, nullptr, 0u);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "unreplayable: command 1 imports guest memory") != nullptr);
  /* The command after it never reached the library. */
  CHECK_EQ(ops.executed(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_blob_imported_into_a_context_leaves_it_unreplayable)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  written.recorder().record_blob(3u, BlobAction::imported, 0u, 9u, 0u, 4096u);
  Batch batch;
  batch.add(0x00010006u, 4u);
  written.ran(3u, 6u, batch, nullptr, 0u);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "holds no bytes for the blob") != nullptr);
  CHECK_EQ(ops.executed(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, more_contexts_than_it_holds_is_a_finding_and_the_rest_still_replay)
{ Written written;
  for (std::uint32_t context = 1u; context <= RecordingReplay::max_contexts + 2u; context++)
      written.set_up(context, SubApi::vulkan);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.contexts(), RecordingReplay::max_contexts);
  CHECK_EQ(replay.findings(), 2u);
  CHECK(std::strstr(replay.finding(0u), "more contexts than this replay holds") != nullptr);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_damaged_recording_is_malformed_and_replays_what_it_read)
{ Written written;
  one_batch(written, 0x77u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length() - 3u);
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::malformed);
  CHECK_EQ(replay.contexts(), 1u);
  CHECK(replay.invariants_hold());
  /* A file that is no recording at all replays nothing. */
  const std::uint8_t nothing[8] = {};
  RecordingReader    empty(nothing, sizeof nothing);
  RecordingReplay    none(library);
  CHECK_STATUS(none.run(empty, filter), Status::malformed);
  CHECK_EQ(none.contexts(), 0u); }

TEST(recording_replay_run, a_filter_narrows_what_is_replayed)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  written.set_up(4u, SubApi::vulkan);
  Batch batch;
  batch.add(0x00010006u, 4u);
  written.ran(3u, 1u, batch, nullptr, 0u);
  written.ran(4u, 2u, batch, nullptr, 0u);
  written.close();
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  RecordingFilter filter;
  filter.keep_context(4u);
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.contexts(), 1u);
  CHECK(replay.commands() == 1u);
  CHECK_EQ(replay.findings(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(replay_context, holds_one_executor_and_compares_the_answer_it_produced)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  ReplayContext context;
  CHECK(!context.created());
  CHECK(!context.answered());
  CHECK(context.invariants_hold());
  CHECK_STATUS(context.create(library, 3u, SubApi::vulkan, uuid), Status::ok);
  CHECK(context.created());
  CHECK_EQ(context.context(), 3u);
  CHECK_EQ(context.command_api(), 1u);
  CHECK_STATUS(context.create(library, 3u, SubApi::vulkan, uuid), Status::invalid_state);
  CHECK(context.invariants_hold());
  CHECK(!context.gave_up());
  context.give_up();
  CHECK(context.gave_up());
  context.destroy(library);
  CHECK(!context.created());
  context.destroy(library);
  CHECK_EQ(ops.destroyed(), 1u);
  CHECK(context.invariants_hold()); }

TEST(replay_context, names_each_sub_apis_command_half)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN | RVK_EXECUTOR_SUB_API_OPENCL
                      | RVK_EXECUTOR_SUB_API_OPENGL);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  struct Case { SubApi api; std::uint16_t half; };
  const Case cases[] = {{SubApi::vulkan, 1u}, {SubApi::opencl, 2u}, {SubApi::opengl, 3u}};
  for (const Case &c : cases) {
      ReplayContext context;
      CHECK_STATUS(context.create(library, 1u, c.api, uuid), Status::ok);
      CHECK_EQ(context.command_api(), c.half);
      context.destroy(library);
  } }

TEST(replay_context, keeps_the_last_answer_asked_for_and_forgets_it_between_batches)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  ReplayContext context;
  CHECK_STATUS(context.create(library, 3u, SubApi::vulkan, uuid), Status::ok);

  Batch batch;
  batch.add(0x00010006u, 4u, CommandHeader::generate_reply, 0x2Au)
       .add(0x00010007u, 4u, 0u, 0x2Bu);
  av::RecordedCommand command;
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
  CHECK_STATUS(context.execute(library, command), Status::ok);
  CHECK(context.answered());
  CHECK_EQ(context.answer_length(), 4u);
  const std::uint8_t same[4] = {0x2Au, 0x2Au, 0x2Au, 0x2Au};
  CHECK(context.answers_with(same, sizeof same));
  const std::uint8_t other[4] = {0x2Au, 0x2Bu, 0x2Au, 0x2Au};
  CHECK(!context.answers_with(other, sizeof other));
  CHECK(!context.answers_with(same, 3u));
  CHECK(context.invariants_hold());

  /* A command that asked for no answer leaves the last one alone. */
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), command.next_offset()), Status::ok);
  CHECK_STATUS(context.execute(library, command), Status::ok);
  CHECK(context.answers_with(same, sizeof same));

  context.forget_answer();
  CHECK(!context.answered());
  CHECK_EQ(context.answer_length(), 0u);
  CHECK(!context.answers_with(same, sizeof same));
  /* A batch that produced no answer matches no recorded reply, not even an
   * empty one: the recording says a command asked and the replay says none did. */
  CHECK(!context.answers_with(same, 0u));
  CHECK(context.invariants_hold());
  context.destroy(library); }

TEST(replay_context_execute, tells_each_of_the_librarys_refusals_apart)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  struct Case { std::uint32_t id; Status status; };
  const Case cases[] = {{MockExecutorOps::unknown_id, Status::not_found},
                        {MockExecutorOps::undecodable_id, Status::malformed},
                        {MockExecutorOps::lost_id, Status::invalid_state},
                        {MockExecutorOps::garbage_id, Status::invalid_state}};
  for (const Case &c : cases) {
      ReplayContext context;
      CHECK_STATUS(context.create(library, 3u, SubApi::vulkan, uuid), Status::ok);
      Batch batch;
      batch.add(c.id, 0u);
      av::RecordedCommand command;
      CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
      CHECK_STATUS(context.execute(library, command), c.status);
      CHECK(context.invariants_hold());
      context.destroy(library);
  }
  /* A context with no executor runs nothing. */
  ReplayContext none;
  Batch         batch;
  batch.add(0x00010006u, 0u);
  av::RecordedCommand command;
  CHECK_STATUS(command.decode(batch.bytes(), batch.length(), 0u), Status::ok);
  CHECK_STATUS(none.execute(library, command), Status::invalid_state); }
