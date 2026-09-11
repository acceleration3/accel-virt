/* One context's batches run through its executor on its worker: in order,
 * unknown commands skipped, the last answer asked for written to the reply
 * slot, and every stream no producer could frame refused. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/batch_executor.hpp"
#include "device/execution/batch_staging.hpp"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/memory/blob_window.hpp"
#include "device/ring/ring_window.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "harness/ring/guest_ring.hpp"
#include "proto/context/sub_api.hpp"
#include "proto/submission/batch_header.hpp"
#include "proto/submission/command_header.hpp"
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

}  // namespace av

namespace {

constexpr std::uint8_t  uuid[16] = {3};
constexpr std::uint32_t reply_bytes = 64u;

/* Commands laid out as a producer frames them. */
class Commands {
public:
    Commands &add(std::uint32_t id, std::uint32_t payload_bytes, std::uint8_t flags = 0u, std::uint8_t fill = 0u)
    {
        static_cast<void>(CommandHeader(id, 8u + payload_bytes, flags).encode(bytes_ + size_, 8u));
        std::memset(bytes_ + size_ + 8u, fill, payload_bytes);
        size_ += 8u + payload_bytes;
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
          reply_{}, reply_pointer_(reply_), reply_length_(slot_bytes), ops_(RVK_EXECUTOR_SUB_API_VULKAN),
          executor_(host_, library_), staging_(host_), batches_(host_, executor_, staging_)
    {
        std::memset(reply_, 0xEE, sizeof reply_);
        static_cast<void>(library_.accept(&ops_.table()));
        executor_.configure(SubApi::vulkan, uuid);
        static_cast<void>(staging_.allocate(4096u, slot_bytes > 32u ? slot_bytes - 32u : 0u));
        batches_.prepare(slot_bytes != 0u ? BlobWindow(&reply_pointer_, &reply_length_, 1u, reply_length_)
                                          : BlobWindow());
    }

    MockHost        &host() { return host_; }
    MockExecutorOps &ops() { return ops_; }
    BatchExecutor   &batches() { return batches_; }
    GuestRing       &guest() { return guest_; }
    const std::uint8_t *reply() const { return reply_; }
    ReplyHeader      reply_header() const
    {
        ReplyHeader header;
        static_cast<void>(header.decode(reply_, 32u));
        return header;
    }

    /* The next batch at the guest's tail, run on a worker: the status run gave. */
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
    MockHost        host_;
    MockExecutorOps ops_;
    ExecutorLibrary library_;
    ContextExecutor executor_;
    BatchStaging    staging_;
    BatchExecutor   batches_;
};

}  // namespace

TEST(batch_executor_run, runs_the_context_api_commands_in_order_and_skips_the_rest)
{ Fixture fixture;
  Commands commands;
  commands.add(0x10001u, 3u).add(0x00004u, 0u).add(0x20001u, 5u).add(MockExecutorOps::unknown_id, 1u)
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
  CHECK_EQ(fixture.reply()[0], 0xEEu);   /* nothing asked for an answer */
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
  ops.editable().execute = [](void *, RvkExecutor *, std::uint32_t, const std::uint8_t *, std::size_t, std::uint8_t *,
                              std::size_t, std::size_t *reply_size) -> std::uint32_t {
      *reply_size = std::size_t{1} << 33;
      return RVK_EXECUTED;
  };
  Commands commands;
  commands.add(0x10001u, 0u, CommandHeader::generate_reply);
  CHECK_STATUS(fixture.run(1u, commands), Status::ok);
  CHECK(fixture.reply_header().too_large());
  CHECK_EQ(fixture.reply_header().required(), 0xFFFFFFFFu); }

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
       Status::invalid_state},
  };
  for (const Case &c : cases) {
      Fixture fixture;
      Commands commands;
      commands.add(0x10001u, 2u);
      c.build(commands);
      commands.add(0x10005u, 0u);
      if (fixture.run(1u, commands) != c.status)
          av::TestRegistry::instance().record_failure(__FILE__, __LINE__, c.name);
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
  CHECK_EQ(fixture.ops().executed(), 0u); }
