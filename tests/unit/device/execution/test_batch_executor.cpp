#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/batch_executor.hpp"
#include "device/recording/wire_recorder.hpp"
#include "proto/context/gpu_identity.hpp"
#include "proto/context/protocol_version.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/blob_imports.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "proto/context/gl_context_options.hpp"
#include "device/execution/executor_shares.hpp"
#include "device/fence/fence_interrupts.hpp"
#include "device/fence/fence_reporter.hpp"
#include "device/fence/fence_window.hpp"
#include "device/memory/host_blob_table.hpp"
#include "device/memory/blob_window.hpp"
#include "device/context/ring_window.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/recording/recorded_file.hpp"
#include "harness/context/guest_ring.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/blob_import_answer.hpp"
#include "proto/submission/blob_import_request.hpp"
#include "proto/submission/command_header.hpp"
#include "proto/submission/fault_code.hpp"
#include "proto/submission/fence_page.hpp"
#include "proto/submission/reply_header.hpp"

#include <cstdint>
#include <cstring>

using av::BatchExecutor;
using av::BatchHeader;
using av::BatchStaging;
using av::BlobWindow;
using av::CommandHeader;
using av::ContextExecutor;
using av::ExecutorLibrary;
using av::FaultCode;
using av::FencePage;
using av::GuestRing;
using av::HostViolation;
using av::MockExecutorOps;
using av::MockHost;
using av::ReplyHeader;
using av::RingWindow;
using av::Status;
using av::SubApi;

namespace av {

struct BatchExecutorProbe {
    static void overcount(BatchExecutor &executor) { executor.skipped_ = 1u; executor.commands_ = 0u; executor.batches_ = 0u; }
    static void run_without_batch(BatchExecutor &executor) { executor.commands_ = 1u; executor.batches_ = 0u; }
    static void break_reply(BatchExecutor &executor) { executor.reply_ = BlobWindow(nullptr, nullptr, 1u, 64u); }
};

}

namespace {

constexpr std::uint8_t  uuid[16] = {3};
constexpr std::uint32_t reply_bytes = 64u;

class Commands {
public:
    Commands &add(std::uint32_t id, std::uint32_t payload_bytes, std::uint8_t flags = 0u, std::uint8_t fill = 0u)
    {
        static_cast<void>(CommandHeader(id, 8u + payload_bytes, flags).encode(bytes_ + size_, 8u));
        std::memset(bytes_ + size_ + 8u, fill, payload_bytes);
        size_ += 8u + payload_bytes;
        return *this;
    }
    Commands &add_body(std::uint32_t id, const std::uint8_t *body, std::uint32_t length, std::uint8_t flags)
    {
        static_cast<void>(CommandHeader(id, 8u + length, flags).encode(bytes_ + size_, 8u));
        std::memcpy(bytes_ + size_ + 8u, body, length);
        size_ += 8u + length;
        return *this;
    }
    Commands &tail(std::uint32_t count)
    {
        size_ += count;
        return *this;
    }
    Commands &raw(std::uint32_t word_one, std::uint32_t word_two)
    {
        std::memcpy(bytes_ + size_, &word_one, 4u);
        std::memcpy(bytes_ + size_ + 4u, &word_two, 4u);
        size_ += 8u;
        return *this;
    }
    const std::uint8_t *bytes() const { return bytes_; }
    std::uint32_t       size() const { return size_; }

private:
    std::uint8_t  bytes_[4096] = {};
    std::uint32_t size_ = 0u;
};

class Fixture {
public:
    explicit Fixture(std::uint32_t slot_bytes = 32u + reply_bytes)
        : ring_bytes_{}, ring_pointer_(ring_bytes_), ring_length_(sizeof ring_bytes_), guest_(ring_bytes_, 12u),
          reply_{}, reply_pointer_(reply_), reply_length_(slot_bytes), page_{}, page_pointer_(page_),
          page_length_(sizeof page_), ops_(AV_EXECUTOR_SUB_API_VULKAN), shares_(host_, library_), executor_(host_, library_, shares_, 1u), blobs_(host_),
          recorder_(host_), imports_(host_, blobs_, executor_, recorder_, 1u), staging_(host_), interrupts_(host_),
          fences_(host_, interrupts_, recorder_, 1u),
          batches_(host_, executor_, imports_, staging_, fences_, recorder_, 1u)
    {
        std::memset(reply_, 0xEE, sizeof reply_);
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(uuid));
        static_cast<void>(fences_.prepare(av::FenceWindow(BlobWindow(&page_pointer_, &page_length_, 1u, page_length_)), 0u));
        executor_.configure(SubApi::vulkan, uuid, av::GlContextOptions());
        static_cast<void>(staging_.allocate(4096u, slot_bytes > 32u ? slot_bytes - 32u : 0u));
        batches_.prepare(slot_bytes != 0u ? BlobWindow(&reply_pointer_, &reply_length_, 1u, reply_length_)
                                          : BlobWindow());
    }

    MockHost        &host() { return host_; }
    std::uint64_t    progress() const
    {
        FencePage page;
        static_cast<void>(page.decode(page_, sizeof page_));
        return page.progress();
    }
    MockExecutorOps &ops() { return ops_; }
    BatchExecutor   &batches() { return batches_; }
    av::WireRecorder &recorder() { return recorder_; }
    void record_to(const char *path)
    {
        CHECK_STATUS(recorder_.open(path, 0u, "all", av::AtCeiling::stop, av::GpuIdentity(),
                                    av::ProtocolVersion(1u, 1u)),
                     Status::ok);
    }
    GuestRing       &guest() { return guest_; }
    const std::uint8_t *reply() const { return reply_; }
    ReplyHeader      reply_header() const
    {
        ReplyHeader header;
        static_cast<void>(header.decode(reply_, 32u));
        return header;
    }

    Status run(std::uint64_t fence, const Commands &commands)
    {
        const std::uint64_t head = guest_.tail();
        guest_.write_batch(fence, commands.bytes(), commands.size());
        struct Call {
            Fixture      *fixture;
            std::uint64_t head;
            BatchHeader   batch;
            Status        status;
        } call{this, head, BatchHeader(commands.size(), fence), Status::ok};
        on_worker([](void *context) {
            auto *const c = static_cast<Call *>(context);
            RingWindow ring(BlobWindow(&c->fixture->ring_pointer_, &c->fixture->ring_length_, 1u,
                                       c->fixture->ring_length_), 12u);
            if (!c->fixture->batches_.started()) static_cast<void>(c->fixture->batches_.start());
            c->status = c->fixture->batches_.run(ring, c->head, c->batch);
        }, &call);
        CHECK(batches_.invariants_hold());
        return call.status;
    }

    void on_worker(void (*body)(void *), void *context)
    {
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread(body, context, thread), Status::ok);
        host_.join_thread(thread);
    }

    struct Running {
        Fixture      *fixture;
        std::uint64_t head;
        BatchHeader   batch;
        Status        status;
        void         *thread;
    };

    void begin(Running &running, std::uint64_t fence, const Commands &commands)
    {
        running = Running{this, guest_.tail(), BatchHeader(commands.size(), fence), Status::ok, nullptr};
        guest_.write_batch(fence, commands.bytes(), commands.size());
        CHECK_STATUS(host_.start_thread([](void *context) {
            auto *const r = static_cast<Running *>(context);
            RingWindow  ring(BlobWindow(&r->fixture->ring_pointer_, &r->fixture->ring_length_, 1u,
                                        r->fixture->ring_length_), 12u);
            if (!r->fixture->batches_.started()) static_cast<void>(r->fixture->batches_.start());
            r->status = r->fixture->batches_.run(ring, r->head, r->batch);
        }, &running, running.thread), Status::ok);
    }

    void finish(Running &running) { host_.join_thread(running.thread); }

    ~Fixture()
    {
        on_worker([](void *context) { static_cast<Fixture *>(context)->batches_.stop(); }, this);
    }

private:
    std::uint8_t    ring_bytes_[32u + 4096u];
    void           *ring_pointer_;
    std::uint64_t   ring_length_;
    GuestRing       guest_;
    std::uint8_t    reply_[32u + 4096u];
    void           *reply_pointer_;
    std::uint64_t   reply_length_;
    std::uint8_t    page_[FencePage::encoded_size];
    void           *page_pointer_;
    std::uint64_t   page_length_;
    MockHost        host_;
    MockExecutorOps ops_;
    ExecutorLibrary library_;
    av::ExecutorShares shares_;
    ContextExecutor executor_;
    av::HostBlobTable blobs_;
    av::WireRecorder recorder_;
    av::BlobImports imports_;
    BatchStaging    staging_;
    av::FenceInterrupts interrupts_;
    av::FenceReporter   fences_;
    BatchExecutor   batches_;
};

struct WriteCombined {
    MockHost     *host;
    std::uint8_t  arena[64];
};
WriteCombined *write_combined = nullptr;

std::uint32_t store_write_combined(void *, AvExecutor *, std::uint32_t, const std::uint8_t *payload, std::size_t size,
                                   std::uint8_t *reply, std::size_t capacity, std::size_t *reply_size)
{
    write_combined->host->hold_store(write_combined->arena, payload, size);
    if (reply != nullptr) std::memcpy(reply, payload, size < capacity ? size : capacity);
    *reply_size = reply != nullptr ? size : 0u;
    return AV_EXECUTED;
}

constexpr std::uint8_t ended_word[8] = {1, 2, 3, 4, 5, 6, 7, 8};

void store_at_end(void *, AvExecutor *)
{
    write_combined->host->hold_store(write_combined->arena + 32u, ended_word, sizeof ended_word);
}

constexpr std::size_t reply_fence_at = 8u;

}

TEST(batch_executor_run, runs_the_context_api_commands_in_order_and_skips_the_rest)
{ Fixture fixture;
  Commands commands;
  commands.add(0x10001u, 3u).add(0x0000Eu, 0u).add(0x20001u, 5u).add(MockExecutorOps::unknown_id, 1u)
          .add(0x10009u, 40u);
  CHECK_STATUS(fixture.run(1u, commands), Status::ok);
  CHECK_EQ(fixture.ops().created(), 1u);
  CHECK_EQ(fixture.ops().executed(), 3u);
  CHECK_EQ(fixture.ops().id_at(0u), 0x10001u);
  CHECK_EQ(fixture.ops().size_at(0u), 3u);
  CHECK_EQ(fixture.ops().id_at(1u), MockExecutorOps::unknown_id);
  CHECK_EQ(fixture.ops().id_at(2u), 0x10009u);
  CHECK_EQ(fixture.ops().size_at(2u), 40u);
  CHECK_EQ(fixture.batches().commands(), 2u);
  CHECK_EQ(fixture.batches().skipped(), 3u);
  CHECK_EQ(fixture.progress(), 1u);
  CHECK(fixture.batches().fault() == FaultCode::none);
  CHECK_EQ(fixture.reply()[0], 0xEEu);
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(batch_executor_run, the_last_answer_asked_for_lands_in_the_reply_slot)
{ Fixture fixture;
  Commands commands;
  commands.add(0x10001u, 4u, CommandHeader::generate_reply, 0x11u)
          .add(0x10002u, 6u, CommandHeader::generate_reply, 0x22u)
          .add(0x10003u, 9u, 0u, 0x33u);
  CHECK_STATUS(fixture.run(7u, commands), Status::ok);
  const ReplyHeader header = fixture.reply_header();
  CHECK(!header.too_large());
  CHECK_EQ(header.fence_id(), 7u);
  CHECK_EQ(header.size(), 6u);
  CHECK_EQ(fixture.reply()[32], 0x22u);
  CHECK_EQ(fixture.reply()[37], 0x22u);
  CHECK_EQ(fixture.reply()[38], 0xEEu); }

TEST(batch_executor_run, a_nop_asking_for_an_answer_gets_an_empty_one_and_never_reaches_the_executor)
{ Fixture fixture;
  Commands commands;
  commands.add(CommandHeader::nop_id, 0u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(5u, commands), Status::ok);
  CHECK_EQ(fixture.ops().created(), 1u);
  CHECK_EQ(fixture.ops().executed(), 0u);
  const ReplyHeader header = fixture.reply_header();
  CHECK_EQ(fixture.reply()[0], 0x41u);
  CHECK(!header.too_large());
  CHECK_EQ(header.fence_id(), 5u);
  CHECK_EQ(header.size(), 0u);
  CHECK_EQ(header.required(), 0u);
  CHECK_EQ(fixture.reply()[32], 0xEEu);
  CHECK_EQ(fixture.batches().commands(), 1u);
  CHECK_EQ(fixture.batches().skipped(), 0u);
  CHECK_EQ(fixture.progress(), 1u);
  CHECK(fixture.batches().fault() == FaultCode::none); }

TEST(batch_executor_run, a_nop_not_asking_for_an_answer_writes_nothing)
{ Fixture fixture;
  Commands commands;
  commands.add(CommandHeader::nop_id, 0u).add(CommandHeader::nop_id, 0u);
  CHECK_STATUS(fixture.run(1u, commands), Status::ok);
  CHECK_EQ(fixture.ops().executed(), 0u);
  CHECK_EQ(fixture.batches().commands(), 2u);
  CHECK_EQ(fixture.reply()[0], 0xEEu);
  Fixture slotless(0u);
  CHECK_STATUS(slotless.run(1u, commands), Status::ok);
  CHECK_EQ(slotless.batches().commands(), 2u); }

TEST(batch_executor_run, a_nop_is_the_last_answer_only_when_it_comes_last)
{ Fixture fixture;
  Commands nop_last;
  nop_last.add(0x10001u, 6u, CommandHeader::generate_reply, 0x22u).add(CommandHeader::nop_id, 0u,
                                                                     CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(1u, nop_last), Status::ok);
  CHECK_EQ(fixture.reply_header().fence_id(), 1u);
  CHECK_EQ(fixture.reply_header().size(), 0u);
  Commands nop_first;
  nop_first.add(CommandHeader::nop_id, 0u, CommandHeader::generate_reply)
           .add(0x10001u, 6u, CommandHeader::generate_reply, 0x33u);
  CHECK_STATUS(fixture.run(2u, nop_first), Status::ok);
  CHECK_EQ(fixture.reply_header().fence_id(), 2u);
  CHECK_EQ(fixture.reply_header().size(), 6u);
  CHECK_EQ(fixture.reply()[32], 0x33u);
  CHECK_EQ(fixture.ops().executed(), 2u); }

TEST(batch_executor_run, a_nop_with_a_body_faults_the_context_as_misframed)
{ const std::uint32_t bodies[] = {1u, 7u, 8u, 4000u};
  const std::uint8_t  flags[] = {0u, CommandHeader::generate_reply};
  for (const std::uint32_t body : bodies) {
      for (const std::uint8_t flag : flags) {
          Fixture fixture;
          Commands commands;
          commands.add(0x10001u, 2u).add(CommandHeader::nop_id, body, flag).add(0x10005u, 0u, flag);
          CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
          CHECK(fixture.batches().fault() == FaultCode::command_framing);
          CHECK_EQ(fixture.ops().executed(), 1u);
          CHECK_EQ(fixture.batches().commands(), 1u);
          CHECK_EQ(fixture.progress(), 1u);
          CHECK_EQ(fixture.reply()[0], 0xEEu);
      }
  }
  Fixture slotless(0u);
  Commands asking;
  asking.add(CommandHeader::nop_id, 0u, CommandHeader::generate_reply);
  CHECK_STATUS(slotless.run(1u, asking), Status::malformed);
  CHECK(slotless.batches().fault() == FaultCode::command_framing); }

TEST(batch_executor_run, another_transport_id_is_skipped_like_one_the_executor_does_not_know)
{ Fixture fixture;
  Commands commands;
  commands.add(0x00000000u, 0u, CommandHeader::generate_reply)
          .add(0x0000000Eu, 0u, CommandHeader::generate_reply)
          .add(0x0000FFFFu, 12u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(1u, commands), Status::ok);
  CHECK_EQ(fixture.ops().executed(), 0u);
  CHECK_EQ(fixture.batches().skipped(), 3u);
  CHECK_EQ(fixture.batches().commands(), 0u);
  CHECK_EQ(fixture.progress(), 0u);
  CHECK_EQ(fixture.reply()[0], 0xEEu);
  CHECK(fixture.batches().fault() == FaultCode::none); }

TEST(batch_executor_run, an_answer_larger_than_the_slot_says_what_it_needed)
{ Fixture fixture;
  Commands commands;
  commands.add(0x10001u, reply_bytes + 1u, CommandHeader::generate_reply, 0x44u);
  CHECK_STATUS(fixture.run(2u, commands), Status::ok);
  CHECK(fixture.reply_header().too_large());
  CHECK_EQ(fixture.reply_header().required(), reply_bytes + 1u);
  CHECK_EQ(fixture.reply()[32], 0xEEu);
  Commands oversized;
  oversized.add(MockExecutorOps::oversized_id, 0u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(3u, oversized), Status::ok);
  CHECK(fixture.reply_header().too_large());
  CHECK_EQ(fixture.reply_header().fence_id(), 3u);
  Commands fits;
  fits.add(0x10001u, reply_bytes, CommandHeader::generate_reply, 0x55u);
  CHECK_STATUS(fixture.run(4u, fits), Status::ok);
  CHECK(!fixture.reply_header().too_large());
  CHECK_EQ(fixture.reply()[32u + reply_bytes - 1u], 0x55u); }

TEST(batch_executor_run, an_answer_past_four_gigabytes_is_reported_as_the_largest_it_can_say)
{ Fixture fixture;
  MockExecutorOps &ops = fixture.ops();
  ops.editable().execute = [](void *, AvExecutor *, std::uint32_t, const std::uint8_t *, std::size_t, std::uint8_t *,
                              std::size_t, std::size_t *reply_size) -> std::uint32_t {
      *reply_size = std::size_t{1} << 33;
      return AV_EXECUTED;
  };
  Commands commands;
  commands.add(0x10001u, 0u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(1u, commands), Status::ok);
  CHECK(fixture.reply_header().too_large());
  CHECK_EQ(fixture.reply_header().required(), 0xFFFFFFFFu); }

TEST(batch_executor_run, what_a_command_stored_write_combined_is_in_memory_before_its_answer_is)
{ Fixture fixture;
  WriteCombined stored{&fixture.host(), {}};
  write_combined = &stored;
  fixture.ops().editable().execute = &store_write_combined;
  fixture.host().watch(fixture.reply() + reply_fence_at);
  Commands commands;
  commands.add(0x10001u, 16u, 0u, 0x11u).add(0x10002u, 24u, CommandHeader::generate_reply, 0x5Au);
  CHECK_STATUS(fixture.run(9u, commands), Status::ok);
  CHECK_EQ(fixture.reply_header().fence_id(), 9u);
  CHECK_EQ(fixture.reply_header().size(), 24u);
  for (std::uint32_t at = 0u; at < 24u; at++) CHECK_EQ(stored.arena[at], 0x5Au);
  CHECK_EQ(stored.arena[24], 0u);
  CHECK_EQ(fixture.host().watched_when_landed(), 0xEEEEEEEEEEEEEEEEu);
  write_combined = nullptr; }

TEST(batch_executor_run, what_a_batch_stored_write_combined_is_in_memory_once_it_has_run)
{ Fixture fixture;
  WriteCombined stored{&fixture.host(), {}};
  write_combined = &stored;
  fixture.ops().editable().execute = &store_write_combined;
  Commands commands;
  commands.add(0x10001u, 40u, 0u, 0x33u);
  CHECK_STATUS(fixture.run(2u, commands), Status::ok);
  CHECK_EQ(fixture.reply()[0], 0xEEu);
  for (std::uint32_t at = 0u; at < 40u; at++) CHECK_EQ(stored.arena[at], 0x33u);
  CHECK_EQ(stored.arena[40], 0u);
  write_combined = nullptr; }

TEST(batch_executor_run, what_the_end_of_a_batch_stored_write_combined_is_in_memory_once_it_has_run)
{ Fixture fixture;
  WriteCombined stored{&fixture.host(), {}};
  write_combined = &stored;
  fixture.ops().editable().end_batch = &store_at_end;
  Commands commands;
  commands.add(0x10001u, 8u, CommandHeader::generate_reply, 0x44u);
  CHECK_STATUS(fixture.run(3u, commands), Status::ok);
  CHECK_EQ(fixture.reply_header().fence_id(), 3u);
  CHECK(std::memcmp(stored.arena + 32u, ended_word, sizeof ended_word) == 0);
  write_combined = nullptr; }

TEST(batch_executor_run, a_batch_with_no_commands_runs_nothing)
{ Fixture fixture;
  CHECK_STATUS(fixture.run(1u, Commands()), Status::ok);
  CHECK_EQ(fixture.ops().executed(), 0u);
  CHECK_EQ(fixture.batches().batches(), 1u); }

TEST(batch_executor_run, commands_across_the_end_of_the_ring_are_read_whole)
{ Fixture fixture;
  Commands filler;
  filler.add(0x10001u, 4000u);
  CHECK_STATUS(fixture.run(1u, filler), Status::ok);
  Commands wrapped;
  wrapped.add(0x10002u, 60u, CommandHeader::generate_reply, 0x66u);
  CHECK_STATUS(fixture.run(2u, wrapped), Status::ok);
  CHECK(fixture.guest().tail() > 4096u);
  CHECK_EQ(fixture.reply_header().size(), 60u);
  for (std::uint32_t i = 0u; i < 60u; i++) CHECK_EQ(fixture.reply()[32u + i], 0x66u); }

TEST(batch_executor_run, a_stream_no_producer_could_frame_or_an_executor_failure_is_refused)
{ struct Case {
      const char *name;
      void      (*build)(Commands &commands);
      Status      status;
  };
  const Case cases[] = {
      {"past the batch", [](Commands &c) { c.raw(0x10002u, 200u); }, Status::malformed},
      {"below the header", [](Commands &c) { c.raw(0x10002u, 7u); }, Status::malformed},
      {"unknown flag", [](Commands &c) { c.raw(0x10002u, 0x02000008u); }, Status::malformed},
      {"short tail", [](Commands &c) { c.tail(5u); }, Status::malformed},
      {"undecodable", [](Commands &c) { c.add(MockExecutorOps::undecodable_id, 0u); }, Status::malformed},
      {"device lost", [](Commands &c) { c.add(MockExecutorOps::lost_id, 0u); }, Status::invalid_state},
      {"an answer no library gives", [](Commands &c) { c.add(MockExecutorOps::garbage_id, 0u); },
       Status::not_supported},
  };
  const FaultCode faults[] = {FaultCode::command_framing, FaultCode::command_framing, FaultCode::command_framing,
                             FaultCode::command_framing, FaultCode::undecodable, FaultCode::device_lost,
                             FaultCode::broken_executor};
  std::uint32_t index = 0u;
  for (const Case &c : cases) {
      Fixture fixture;
      Commands commands;
      commands.add(0x10001u, 2u);
      c.build(commands);
      commands.add(0x10005u, 0u);
      if (fixture.run(1u, commands) != c.status || fixture.batches().fault() != faults[index++])
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
      CHECK_EQ(fixture.progress(), 1u);
      CHECK_EQ(fixture.ops().id_at(1u) == 0x10005u, false);
  } }

TEST(batch_executor_run, an_answer_asked_for_without_a_reply_slot_is_refused)
{ Fixture fixture(0u);
  Commands quiet;
  quiet.add(0x10001u, 4u);
  CHECK_STATUS(fixture.run(1u, quiet), Status::ok);
  Commands asking;
  asking.add(0x10001u, 4u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(2u, asking), Status::malformed);
  CHECK(fixture.batches().fault() == FaultCode::command_framing);
  CHECK_EQ(fixture.ops().executed(), 1u); }

TEST(batch_executor_start, makes_the_executor_once_and_stop_releases_it)
{ Fixture fixture;
  fixture.on_worker([](void *context) {
      auto *const f = static_cast<Fixture *>(context);
      CHECK_STATUS(f->batches().start(), Status::ok);
      CHECK(f->batches().started());
      CHECK_STATUS(f->batches().start(), Status::invalid_state);
      f->batches().stop();
      CHECK(!f->batches().started());
      f->ops().fail_create_at(1u);
      CHECK_STATUS(f->batches().start(), Status::no_memory);
      CHECK_STATUS(f->batches().run(RingWindow(), 0u, BatchHeader(0u, 1u)), Status::invalid_state);
      CHECK(f->batches().fault() == FaultCode::no_executor);
  }, &fixture);
  CHECK_EQ(fixture.ops().created(), 1u);
  CHECK_EQ(fixture.ops().destroyed(), 1u); }

TEST(batch_executor_prepare, withdraw_forgets_the_reply_slot)
{ Fixture fixture;
  fixture.batches().withdraw();
  Commands asking;
  asking.add(0x10001u, 4u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(1u, asking), Status::malformed); }

TEST(batch_executor, prepares_on_the_main_loop_and_runs_on_a_worker)
{ Fixture fixture;
  CHECK_STATUS(fixture.batches().start(), Status::ok);
  CHECK_STATUS(fixture.batches().run(RingWindow(), 0u, BatchHeader(0u, 1u)), Status::ok);
  fixture.batches().stop();
  for (int count = 0; count < 5; count++) CHECK(fixture.host().claim(HostViolation::worker_required));
  fixture.on_worker([](void *context) {
      auto *const f = static_cast<Fixture *>(context);
      f->batches().withdraw();
      f->batches().prepare(BlobWindow());
  }, &fixture);
  CHECK(fixture.host().claim(HostViolation::main_loop_required));
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(batch_executor_invariants, more_commands_skipped_than_batches_run_is_detected)
{ Fixture fixture;
  CHECK(fixture.batches().invariants_hold());
  av::BatchExecutorProbe::overcount(fixture.batches());
  CHECK(!fixture.batches().invariants_hold());
  Fixture commanded;
  av::BatchExecutorProbe::run_without_batch(commanded.batches());
  CHECK(!commanded.batches().invariants_hold());
  Fixture broken;
  av::BatchExecutorProbe::break_reply(broken.batches());
  CHECK(!broken.batches().invariants_hold());
  broken.batches().withdraw(); }

TEST(batch_executor_run, a_ring_the_blob_cannot_hold_is_refused_before_anything_runs)
{ Fixture fixture;
  std::uint8_t        bytes[40] = {};
  void               *pointer = bytes;
  const std::uint64_t length = sizeof bytes;
  struct Call {
      Fixture      *fixture;
      RingWindow    ring;
      Status        status;
  } call{&fixture, RingWindow(BlobWindow(&pointer, &length, 1u, length), 12u), Status::ok};
  fixture.on_worker([](void *context) {
      auto *const c = static_cast<Call *>(context);
      CHECK_STATUS(c->fixture->batches().start(), Status::ok);
      c->status = c->fixture->batches().run(c->ring, 0u, BatchHeader(64u, 1u));
  }, &call);
  CHECK_STATUS(call.status, Status::malformed);
  CHECK(fixture.batches().fault() == FaultCode::command_framing);
  CHECK_EQ(fixture.ops().executed(), 0u); }

TEST(batch_executor_run, a_refused_import_is_answered_in_the_slot_and_the_context_runs_on)
{ Fixture fixture;
  std::uint8_t body[av::BlobImportRequest::encoded_size] = {};
  CHECK_STATUS(av::BlobImportRequest(77u).encode(body, sizeof body), Status::ok);
  Commands commands;
  commands.add_body(CommandHeader::import_blob_id, body, sizeof body, CommandHeader::generate_reply)
          .add(CommandHeader::release_blob_id, 8u);
  CHECK_STATUS(fixture.run(3u, commands), Status::ok);
  CHECK(fixture.batches().fault() == FaultCode::none);
  CHECK_EQ(fixture.batches().commands(), 2u);
  CHECK_EQ(fixture.ops().executed(), 0u);
  CHECK_EQ(fixture.ops().imported(), 0u);
  const ReplyHeader header = fixture.reply_header();
  CHECK_EQ(header.fence_id(), 3u);
  CHECK_EQ(header.size(), av::BlobImportAnswer::encoded_size);
  av::BlobImportAnswer answer;
  CHECK_STATUS(answer.decode(fixture.reply() + 32u, av::BlobImportAnswer::encoded_size), Status::ok);
  CHECK_EQ(answer.status(), av::BlobImportAnswer::unknown_resource);
  CHECK_EQ(fixture.progress(), 1u); }

TEST(batch_executor_run, a_heartbeat_goes_out_every_256_commands_run)
{ Fixture fixture;
  Commands commands;
  for (std::uint32_t i = 0u; i < 256u; i++) commands.add(0x10001u, 0u);
  CHECK_STATUS(fixture.run(1u, commands), Status::ok);
  CHECK_EQ(fixture.batches().commands(), 256u);
  CHECK_EQ(fixture.progress(), 1u);
  Commands one_more;
  one_more.add(0x10001u, 0u);
  CHECK_STATUS(fixture.run(2u, one_more), Status::ok);
  CHECK_EQ(fixture.batches().commands(), 257u);
  CHECK_EQ(fixture.progress(), 2u); }

TEST(batch_executor_run, a_blob_command_framed_with_the_wrong_body_faults_the_context_as_misframed)
{ const std::uint32_t bodies[][2] = {{CommandHeader::import_blob_id, 24u}, {CommandHeader::import_blob_id, 40u},
                                     {CommandHeader::release_blob_id, 0u}, {CommandHeader::release_blob_id, 16u}};
  for (const auto &body : bodies) {
      Fixture fixture;
      Commands commands;
      commands.add(body[0], body[1], CommandHeader::generate_reply);
      CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
      CHECK(fixture.batches().fault() == FaultCode::command_framing);
      CHECK_EQ(fixture.reply()[0], 0xEEu);
  } }

TEST(batch_executor_run, records_the_batch_before_a_command_runs_and_its_answer_after)
{ Fixture fixture;
  fixture.record_to("batches.av");
  Commands commands;
  commands.add(0x10001u, 4u, CommandHeader::generate_reply, 0x11u).add(0x10002u, 6u, 0u, 0x22u);
  CHECK_STATUS(fixture.run(7u, commands), Status::ok);
  av::RecordedFile   file(fixture.host(), "batches.av");
  av::RecordHeader   record;
  const std::uint8_t *payload = file.expect(av::RecordKind::batch, record);
  CHECK(payload != nullptr);
  CHECK(record.fence() == 7u);
  CHECK_EQ(record.context(), 1u);
  CHECK_EQ(record.length(), commands.size());
  if (payload != nullptr) CHECK_EQ(std::memcmp(payload, commands.bytes(), commands.size()), 0);
  payload = file.expect(av::RecordKind::reply, record);
  CHECK(payload != nullptr);
  CHECK(record.fence() == 7u);
  CHECK_EQ(record.context(), 1u);
  CHECK_EQ(record.length(), 4u);
  if (payload != nullptr) CHECK_EQ(payload[0], 0x11u);
  CHECK_EQ(file.remaining(), 0u);
  fixture.recorder().close(); }

TEST(batch_executor_run, records_the_fault_of_a_batch_that_did_not_complete)
{ Fixture fixture;
  fixture.record_to("faults.av");
  Commands commands;
  commands.add(MockExecutorOps::undecodable_id, 4u);
  CHECK_STATUS(fixture.run(3u, commands), Status::malformed);
  CHECK(fixture.batches().fault() == FaultCode::undecodable);
  av::RecordedFile   file(fixture.host(), "faults.av");
  av::RecordHeader   record;
  CHECK(file.expect(av::RecordKind::batch, record) != nullptr);
  const std::uint8_t *payload = file.expect(av::RecordKind::fault, record);
  CHECK(payload != nullptr);
  CHECK(record.fence() == 3u);
  CHECK_EQ(record.context(), 1u);
  CHECK_EQ(record.length(), 8u);
  if (payload != nullptr) {
      CHECK_EQ(payload[0], static_cast<std::uint8_t>(FaultCode::undecodable));
      for (std::uint32_t index = 1u; index < 8u; index++) CHECK_EQ(payload[index], 0u);
  }
  CHECK_EQ(file.remaining(), 0u);
  fixture.recorder().close(); }

TEST(batch_executor_run, with_recording_off_it_asks_the_host_for_no_recording_at_all)
{ Fixture fixture;
  Commands commands;
  commands.add(0x10001u, 4u, CommandHeader::generate_reply, 0x11u);
  CHECK_STATUS(fixture.run(7u, commands), Status::ok);
  CHECK(!fixture.recorder().recording());
  CHECK(fixture.recorder().records() == 0u);
  CHECK_EQ(fixture.host().recording_length("batches.av"), 0u); }

TEST(batch_executor_run, the_batch_is_on_the_recording_while_its_first_command_is_still_running)
{ Fixture fixture;
  fixture.record_to("crash.av");
  Commands commands;
  commands.add(0x10001u, 4u, CommandHeader::generate_reply, 0x11u);
  fixture.ops().hold();
  Fixture::Running running;
  fixture.begin(running, 11u, commands);
  while (fixture.ops().waiting() == 0u) { }
  { av::RecordedFile   file(fixture.host(), "crash.av");
    av::RecordHeader   record;
    const std::uint8_t *payload = file.expect(av::RecordKind::batch, record);
    CHECK(payload != nullptr);
    if (payload != nullptr) {
        CHECK(record.fence() == 11u);
        CHECK_EQ(std::memcmp(payload, commands.bytes(), commands.size()), 0);
    }
    CHECK_EQ(file.remaining(), 0u); }
  fixture.ops().release();
  fixture.finish(running);
  CHECK_STATUS(running.status, Status::ok);
  CHECK(fixture.batches().invariants_hold());
  fixture.recorder().close(); }

namespace {
Commands &ask_part(Commands &commands, std::uint64_t offset)
{
    std::uint8_t body[8] = {};
    std::memcpy(body, &offset, sizeof body);
    return commands.add_body(CommandHeader::answer_piece_id, body, sizeof body, CommandHeader::generate_reply);
}

}

TEST(batch_executor_run, pieces_across_batches_run_as_one_command_and_its_answer_comes_in_parts)
{ Fixture fixture;
  Commands first;
  first.add(CommandHeader::piece_id, 100u, 0u, 0x01u);
  CHECK_STATUS(fixture.run(1u, first), Status::ok);
  CHECK_EQ(fixture.ops().executed(), 0u);
  Commands second;
  second.add(CommandHeader::piece_id, 100u, 0u, 0x02u).add(0x10003u, 50u, CommandHeader::generate_reply, 0x03u);
  CHECK_STATUS(fixture.run(2u, second), Status::ok);
  CHECK_EQ(fixture.ops().executed(), 1u);
  CHECK_EQ(fixture.ops().id_at(0u), 0x10003u);
  CHECK_EQ(fixture.ops().size_at(0u), 250u);
  CHECK(fixture.reply_header().too_large());
  CHECK_EQ(fixture.reply_header().required(), 250u);
  std::uint8_t  gathered[250] = {};
  std::uint64_t fetched = 0u;
  for (std::uint64_t fence = 3u; fetched < sizeof gathered; fence++) {
      Commands part;
      ask_part(part, fetched);
      CHECK_STATUS(fixture.run(fence, part), Status::ok);
      CHECK(!fixture.reply_header().too_large());
      const std::uint32_t size = fixture.reply_header().size();
      CHECK(size > 8u);
      if (size <= 8u) break;
      std::uint64_t length = 0u;
      std::memcpy(&length, fixture.reply() + 32u, 8u);
      CHECK_EQ(length, 250u);
      std::memcpy(gathered + fetched, fixture.reply() + 40u, size - 8u);
      fetched += size - 8u;
  }
  CHECK_EQ(fetched, 250u);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < 250u; ++index) {
      const std::uint8_t expected = index < 100u ? 0x01u : (index < 200u ? 0x02u : 0x03u);
      if (gathered[index] != expected) wrong++;
  }
  CHECK_EQ(wrong, 0u);
  Commands past;
  ask_part(past, 250u);
  CHECK_STATUS(fixture.run(20u, past), Status::ok);
  CHECK_EQ(fixture.reply_header().size(), 8u);
  CHECK_EQ(fixture.ops().executed(), 1u);
  CHECK(fixture.batches().fault() == FaultCode::none); }

TEST(batch_executor_run, pieces_not_closed_by_their_command_fault_the_context_as_misframed)
{ { Fixture fixture;
    Commands commands;
    commands.add(CommandHeader::piece_id, 10u).add(CommandHeader::nop_id, 0u);
    CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
    CHECK(fixture.batches().fault() == FaultCode::command_framing); }
  { Fixture fixture;
    Commands commands;
    commands.add(CommandHeader::piece_id, 10u).add(0x20001u, 4u);
    CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
    CHECK_EQ(fixture.ops().executed(), 0u); }
  { Fixture fixture;
    Commands commands;
    commands.add(CommandHeader::piece_id, 10u, CommandHeader::generate_reply);
    CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
    CHECK(fixture.batches().fault() == FaultCode::command_framing); }
  { Fixture fixture;
    Commands commands;
    commands.add(CommandHeader::answer_piece_id, 4u, CommandHeader::generate_reply);
    CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
    CHECK(fixture.batches().fault() == FaultCode::command_framing); } }

TEST(batch_executor_run, pieces_the_host_has_no_memory_for_fault_the_context)
{ Fixture fixture;
  fixture.host().fail_allocation_at(1u);
  Commands commands;
  commands.add(CommandHeader::piece_id, 10u).add(0x10001u, 4u);
  CHECK_STATUS(fixture.run(1u, commands), Status::malformed);
  CHECK(fixture.batches().fault() == FaultCode::undecodable);
  CHECK_EQ(fixture.ops().executed(), 0u); }
