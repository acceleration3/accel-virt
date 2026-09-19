#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/executor_library.hpp"
#include "device/recording/recorded_command.hpp"
#include "device/recording/record_header.hpp"
#include "device/recording/record_kind.hpp"
#include "device/recording/record_kinds.hpp"
#include "device/recording/recording_filter.hpp"
#include "device/recording/recording_header.hpp"
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

/* A recording built byte by byte, for the records a device never writes. */
class Handmade {
public:
    Handmade() : length_(0u), bytes_{}
    {
        const av::RecordingHeader header(ProtocolVersion(1u, 1u), av::RecordKinds::every_kind, 0u, 0u, 0u,
                                         0u, uuid);
        CHECK_STATUS(header.encode(bytes_, sizeof bytes_), Status::ok);
        length_ = av::RecordingHeader::encoded_size;
    }

    Handmade &add(av::RecordKind kind, std::uint32_t context, std::uint64_t fence,
                  const std::uint8_t *payload, std::uint32_t payload_length)
    {
        const av::RecordHeader record(kind, context, payload_length, 0u, fence);
        CHECK_STATUS(record.encode(bytes_ + length_, sizeof bytes_ - length_), Status::ok);
        length_ += av::RecordHeader::encoded_size;
        for (std::uint32_t index = 0u; index < payload_length; index++)
            bytes_[length_ + index] = payload[index];
        length_ += payload_length;
        return *this;
    }

    Handmade &setup(std::uint32_t context, SubApi api)
    {
        std::uint8_t bytes[ContextSetup::base_size] = {};
        CHECK_STATUS(ContextSetup(ProtocolVersion(1u, 0u), api, 40u, 16u, 41u, 1u, 42u)
                         .encode(bytes, sizeof bytes),
                     Status::ok);
        return add(av::RecordKind::context_setup, context, 0u, bytes, sizeof bytes);
    }

    const std::uint8_t *bytes() const { return bytes_; }
    std::size_t         length() const { return length_; }

private:
    std::size_t  length_;
    std::uint8_t bytes_[widest];
};

}  // namespace

namespace av {

struct ReplayContextProbe {
    /* An answer the library said was longer than the context can keep. */
    static void answer_too_long(ReplayContext &context)
    {
        context.answered_ = true;
        context.answer_length_ = ReplayContext::answer_bytes + 1u;
    }
    static void keep_a_length_with_no_answer(ReplayContext &context) { context.answer_length_ = 4u; }
    static void answer_without_a_length(ReplayContext &context)
    {
        context.answered_ = false;
        context.answer_length_ = 4u;
    }
    static void forget_the_context(ReplayContext &context) { context.context_ = 0u; }
    static void answer_with_no_executor(ReplayContext &context) { context.answered_ = true; }
};

struct RecordingReplayProbe {
    static void break_a_context(RecordingReplay &replay)
    {
        ReplayContextProbe::forget_the_context(replay.slots_[0]);
    }
    static void unterminate_a_finding(RecordingReplay &replay)
    {
        replay.findings_text_[0][RecordingReplay::finding_bytes - 1u] = 'x';
    }
    static void forget_a_context(RecordingReplay &replay) { replay.contexts_ = 0u; }
};

}  // namespace av

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

/* US3 scenario 1 at the place it is easiest to get wrong: the answer a context
 * kept from one batch must not be compared against the next batch's recorded
 * reply. A second batch that asked for nothing and a recorded reply that says
 * otherwise is a difference, and reporting it as a match would be the replay
 * quietly agreeing with a host it never asked. */
TEST(recording_replay_run, an_answer_is_never_carried_from_one_batch_into_the_next)
{ Written written;
  written.set_up(3u, SubApi::vulkan);
  Batch replying;
  replying.add(0x00010006u, 4u, CommandHeader::generate_reply, 0x7Au);
  const std::uint8_t answer[4] = {0x7Au, 0x7Au, 0x7Au, 0x7Au};
  written.ran(3u, 1u, replying, answer, sizeof answer);
  /* The same bytes recorded against a batch that asks the library for none. */
  Batch silent;
  silent.add(0x00010007u, 4u, 0u, 0x7Au);
  written.ran(3u, 2u, silent, answer, sizeof answer);
  written.close();

  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(ops.executed(), 2u);
  CHECK_EQ(replay.replies(), 2u);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "fence=2") != nullptr);
  CHECK(std::strstr(replay.finding(0u), "replied 0 bytes, recorded 4") != nullptr);
  CHECK(replay.invariants_hold()); }

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
  CHECK(std::strstr(replay.finding(1u), "more contexts than this replay holds") != nullptr);
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
  /* US3 scenario 4: what a recording holds before the damage is replayed, not
   * thrown away with it - the batch that crashed the host is the whole point
   * of replaying a recording that ends in the middle of one. The cut lands in
   * the fence record, so the batch ran and its answer was compared. */
  CHECK_EQ(replay.commands(), 1u);
  CHECK_EQ(replay.replies(), 1u);
  CHECK_EQ(replay.findings(), 0u);
  CHECK_EQ(ops.executed(), 1u);
  CHECK_EQ(ops.id_at(0u), 0x00010006u);
  CHECK(replay.invariants_hold());
  /* A file that is no recording at all replays nothing. */
  const std::uint8_t nothing[8] = {};
  RecordingReader    empty(nothing, sizeof nothing);
  RecordingReplay    none(library);
  CHECK_STATUS(none.run(empty, filter), Status::malformed);
  CHECK_EQ(none.contexts(), 0u);
  CHECK_EQ(none.commands(), 0u);
  CHECK_EQ(ops.executed(), 1u); }

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
      /* And the library was asked for that API, not for the one the last
       * context happened to want. */
      CHECK_EQ(ops.last_sub_api(), static_cast<std::uint32_t>(c.api));
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

TEST(recording_replay_run, a_gpu_of_its_own_is_the_one_it_asks_the_library_for)
{ Written written;
  one_batch(written, 0x33u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(written.bytes(), written.length());
  RecordingReplay replay(library);
  const std::uint8_t other[GpuIdentity::uuid_bytes] = {0x99u};
  replay.use_gpu(other);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(ops.last_uuid_byte(), 0x99u);
  CHECK(replay.invariants_hold()); }

/* The other half of the same sentence: with no GPU named - and with a null one
 * named, which names none - the recording's own is what the library is asked
 * for, so a replay reproduces the run on the GPU it was recorded on. */
TEST(recording_replay_run, with_no_gpu_named_it_asks_the_library_for_the_recordings_own)
{ for (std::uint32_t named = 0u; named < 2u; named++) {
      Written written;
      one_batch(written, 0x33u);
      MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
      ExecutorLibrary library;
      CHECK_STATUS(library.accept(&ops.table()), Status::ok);
      RecordingReader reader(written.bytes(), written.length());
      RecordingReplay replay(library);
      if (named != 0u) replay.use_gpu(nullptr);
      const RecordingFilter filter;
      CHECK_STATUS(replay.run(reader, filter), Status::ok);
      CHECK_EQ(ops.last_uuid_byte(), uuid[0]);
      CHECK_EQ(ops.created(), 1u);
  } }

TEST(recording_replay_run, a_setup_that_does_not_decode_and_a_context_set_up_twice)
{ Handmade           made;
  const std::uint8_t nothing[1] = {};
  made.add(av::RecordKind::context_setup, 1u, 0u, nothing, 0u)
      .setup(2u, SubApi::vulkan)
      .setup(2u, SubApi::vulkan);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "setup does not decode") != nullptr);
  /* The second setup of one context replaces the executor the first made. */
  CHECK_EQ(ops.created(), 2u);
  CHECK_EQ(ops.live(), 1u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_batch_framed_wrong_and_a_transport_command_that_is_not_an_import)
{ Handmade     made;
  std::uint8_t batch[CommandHeader::encoded_size * 2u] = {};
  CHECK_STATUS(CommandHeader(CommandHeader::nop_id, CommandHeader::encoded_size, 0u)
                   .encode(batch, sizeof batch),
               Status::ok);
  /* A second header claiming more than the batch holds. */
  CHECK_STATUS(CommandHeader(0x00010006u, 200u, 0u)
                   .encode(batch + CommandHeader::encoded_size, CommandHeader::encoded_size),
               Status::ok);
  made.setup(1u, SubApi::vulkan).add(av::RecordKind::batch, 1u, 3u, batch, sizeof batch);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "framed wrong from byte 8") != nullptr);
  /* The NOP was skipped rather than handed to the library. */
  CHECK_EQ(ops.executed(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, another_apis_commands_and_a_filter_leave_the_library_alone)
{ Handmade made;
  Batch    batch;
  batch.add(0x00020001u, 4u).add(0x00010007u, 4u).add(0x00010006u, 4u);
  made.setup(1u, SubApi::vulkan).add(av::RecordKind::batch, 1u, 4u, batch.bytes(), batch.length());
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  RecordingFilter filter;
  CHECK_STATUS(filter.keep_command("vkCreateInstance"), Status::ok);
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  /* The OpenCL command is not this context's API, and of the two that are the
   * filter keeps one. */
  CHECK_EQ(ops.executed(), 1u);
  CHECK_EQ(ops.id_at(0u), 0x00010006u);
  CHECK_EQ(replay.findings(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_blob_created_or_released_is_not_an_import_and_replays_nothing)
{ Handmade     made;
  std::uint8_t created[32] = {};
  std::uint8_t released[32] = {};
  released[0] = 2u;
  const std::uint8_t nothing[1] = {};
  made.setup(1u, SubApi::vulkan)
      .add(av::RecordKind::blob, 1u, 0u, created, sizeof created)
      .add(av::RecordKind::blob, 1u, 0u, released, sizeof released)
      .add(av::RecordKind::blob, 1u, 0u, nothing, 0u)          /* too short to read */
      .add(av::RecordKind::blob, 9u, 0u, created, sizeof created);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_blob_imported_into_a_context_it_never_set_up_is_no_finding)
{ Handmade     made;
  std::uint8_t imported[32] = {};
  imported[0] = 1u;
  made.add(av::RecordKind::blob, 9u, 0u, imported, sizeof imported);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, nothing_of_a_context_that_gave_up_is_replayed_or_compared)
{ Handmade     made;
  Batch        batch;
  batch.add(CommandHeader::import_blob_id, 32u);
  std::uint8_t imported[32] = {};
  imported[0] = 1u;
  const std::uint8_t answer[4] = {};
  made.setup(1u, SubApi::vulkan)
      .add(av::RecordKind::batch, 1u, 5u, batch.bytes(), batch.length())
      .add(av::RecordKind::reply, 1u, 5u, answer, sizeof answer)
      .add(av::RecordKind::blob, 1u, 0u, imported, sizeof imported)
      .add(av::RecordKind::batch, 1u, 6u, batch.bytes(), batch.length());
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  /* One finding: the import. The reply, the blob and the second batch all
   * belong to a context that has given up. */
  CHECK_EQ(replay.findings(), 1u);
  CHECK(replay.replies() == 0u);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, a_reply_with_no_answer_behind_it_says_what_it_replied)
{ Handmade           made;
  Batch              batch;
  batch.add(0x00010006u, 4u);              /* asks for no answer */
  const std::uint8_t answer[4] = {};
  made.setup(1u, SubApi::vulkan)
      .add(av::RecordKind::batch, 1u, 7u, batch.bytes(), batch.length())
      .add(av::RecordKind::reply, 1u, 7u, answer, sizeof answer);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(std::strstr(replay.finding(0u), "replied 0 bytes, recorded 4") != nullptr);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_run, counts_more_findings_than_it_can_keep_and_answers_nothing_past_them)
{ Handmade           made;
  const std::uint8_t nothing[1] = {};
  for (std::uint32_t index = 0u; index < RecordingReplay::max_findings + 4u; index++)
      made.add(av::RecordKind::context_setup, index + 1u, 0u, nothing, 0u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), RecordingReplay::max_findings + 4u);
  CHECK(replay.finding(RecordingReplay::max_findings - 1u)[0] != '\0');
  CHECK_EQ(std::strcmp(replay.finding(RecordingReplay::max_findings), ""), 0);
  CHECK_EQ(std::strcmp(replay.finding(RecordingReplay::max_findings + 3u), ""), 0);
  CHECK(replay.invariants_hold()); }

TEST(recording_replay_invariants, catch_a_replay_whose_contexts_or_findings_do_not_agree)
{ Written written;
  one_batch(written, 0x44u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  { RecordingReader reader(written.bytes(), written.length());
    RecordingReplay replay(library);
    const RecordingFilter filter;
    CHECK_STATUS(replay.run(reader, filter), Status::ok);
    CHECK(replay.invariants_hold());
    av::RecordingReplayProbe::break_a_context(replay);
    CHECK(!replay.invariants_hold()); }
  { RecordingReader reader(written.bytes(), written.length());
    RecordingReplay replay(library);
    const RecordingFilter filter;
    CHECK_STATUS(replay.run(reader, filter), Status::ok);
    av::RecordingReplayProbe::forget_a_context(replay);
    CHECK(!replay.invariants_hold()); }
  { RecordingReader reader(written.bytes(), written.length());
    RecordingReplay replay(library);
    RecordingFilter filter;
    filter.keep_context(99u);
    CHECK_STATUS(replay.run(reader, filter), Status::ok);
    av::RecordingReplayProbe::unterminate_a_finding(replay);
    CHECK(replay.invariants_hold());   /* nothing was kept, so nothing to judge */
  } }

TEST(recording_replay_invariants, catch_a_finding_with_no_end)
{ Handmade           made;
  const std::uint8_t nothing[1] = {};
  made.add(av::RecordKind::context_setup, 1u, 0u, nothing, 0u);
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  RecordingReader reader(made.bytes(), made.length());
  RecordingReplay replay(library);
  const RecordingFilter filter;
  CHECK_STATUS(replay.run(reader, filter), Status::ok);
  CHECK_EQ(replay.findings(), 1u);
  CHECK(replay.invariants_hold());
  av::RecordingReplayProbe::unterminate_a_finding(replay);
  CHECK(!replay.invariants_hold()); }

TEST(replay_context, an_answer_longer_than_it_keeps_is_reported_rather_than_compared)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  ReplayContext context;
  CHECK_STATUS(context.create(library, 3u, SubApi::vulkan, uuid), Status::ok);
  av::ReplayContextProbe::answer_too_long(context);
  const std::uint8_t recorded[4] = {};
  CHECK(!context.answers_with(recorded, sizeof recorded));
  /* Even a recorded reply of exactly that length is reported rather than
   * compared: the bytes to compare it against were never kept. */
  CHECK(!context.answers_with(recorded, ReplayContext::answer_bytes + 1u));
  CHECK(context.answered());
  CHECK(context.answer_length() > ReplayContext::answer_bytes);
  context.destroy(library); }

TEST(replay_context_invariants, catch_a_context_that_kept_what_it_could_not_have)
{ MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  ReplayContext context;
  CHECK(context.invariants_hold());
  av::ReplayContextProbe::answer_with_no_executor(context);
  CHECK(!context.invariants_hold());

  ReplayContext kept;
  av::ReplayContextProbe::keep_a_length_with_no_answer(kept);
  CHECK(!kept.invariants_hold());

  ReplayContext second;
  CHECK_STATUS(second.create(library, 3u, SubApi::vulkan, uuid), Status::ok);
  CHECK(second.invariants_hold());
  av::ReplayContextProbe::forget_the_context(second);
  CHECK(!second.invariants_hold());
  second.destroy(library);

  ReplayContext third;
  CHECK_STATUS(third.create(library, 3u, SubApi::vulkan, uuid), Status::ok);
  av::ReplayContextProbe::answer_without_a_length(third);
  CHECK(!third.invariants_hold());
  third.destroy(library); }
