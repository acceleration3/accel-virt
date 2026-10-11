/* One context's host executor: made and released on the context's worker,
 * running commands there, and every answer a library gives read as a status. */
#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "proto/context/gl_context_options.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>
#include <cstring>

using av::ContextExecutor;
using av::ExecutorLibrary;
using av::ExecutorShares;
using av::GlContextOptions;
using av::HostViolation;
using av::MockExecutorOps;
using av::MockHost;
using av::Status;
using av::SubApi;

namespace av {

struct ContextExecutorProbe {
    static void create_unconfigured(ContextExecutor &executor) { executor.configured_ = false; }
};

}  // namespace av

namespace {

constexpr std::uint8_t uuid[16] = {0x5A};

class Fixture {
public:
    Fixture()
        : ops_(AV_EXECUTOR_SUB_API_VULKAN | AV_EXECUTOR_SUB_API_OPENGL), library_(), shares_(host_, library_),
          executor_(host_, library_, shares_, 5u), sharing_(host_, library_, shares_, 6u)
    {
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(uuid));
        static_cast<void>(shares_.initialize());
    }

    MockHost        &host() { return host_; }
    MockExecutorOps &ops() { return ops_; }
    ExecutorShares  &shares() { return shares_; }
    ContextExecutor &executor() { return executor_; }
    /* A second context of the same device, context 6. */
    ContextExecutor &sharing() { return sharing_; }

    template <typename Body>
    void on_worker(Body body)
    {
        struct Call {
            ContextExecutor *executor;
            Body            *body;
        } call{&executor_, &body};
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread([](void *context) {
                         auto *const c = static_cast<Call *>(context);
                         (*c->body)(*c->executor);
                     },
                                        &call, thread),
                     Status::ok);
        host_.join_thread(thread);
        CHECK(executor_.invariants_hold());
    }

private:
    MockHost        host_;
    MockExecutorOps ops_;
    ExecutorLibrary library_;
    ExecutorShares  shares_;
    ContextExecutor executor_;
    ContextExecutor sharing_;
};

}  // namespace

TEST(context_executor_create, makes_the_configured_executor_on_the_worker_and_releases_it_there)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  CHECK(fixture.executor().invariants_hold());
  CHECK_EQ(fixture.executor().command_api(), 1u);
  fixture.on_worker([](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      CHECK_STATUS(executor.create(), Status::invalid_state);
      const std::uint8_t payload[3] = {7u, 8u, 9u};
      std::uint8_t       reply[8] = {};
      std::size_t        reply_size = 99u;
      CHECK_STATUS(executor.execute(0x10001u, payload, sizeof payload, reply, sizeof reply, reply_size), Status::ok);
      CHECK_EQ(reply_size, 3u);
      CHECK_EQ(reply[2], 9u);
      executor.destroy();
      executor.destroy();
  });
  CHECK(!fixture.executor().created());
  CHECK_EQ(fixture.ops().created(), 1u);
  CHECK_EQ(fixture.ops().destroyed(), 1u);
  CHECK_EQ(fixture.ops().last_sub_api(), AV_EXECUTOR_SUB_API_VULKAN);
  CHECK(fixture.ops().last_options().domain != 0u);   /* the device's domain */
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(context_executor_create, an_unconfigured_context_or_a_library_that_makes_nothing_has_no_executor)
{ Fixture fixture;
  fixture.on_worker([](ContextExecutor &executor) { CHECK_STATUS(executor.create(), Status::invalid_state); });
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.ops().fail_create_at(1u);
  fixture.on_worker([](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::no_memory);
      std::size_t reply_size = 0u;
      CHECK_STATUS(executor.execute(0x10001u, nullptr, 0u, nullptr, 0u, reply_size), Status::invalid_state);
  });
  CHECK(!fixture.executor().created()); }

TEST(context_executor_execute, reads_every_answer_a_library_gives)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.on_worker([](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      std::size_t reply_size = 0u;
      struct Case { std::uint32_t id; Status status; };
      const Case cases[] = {
          {MockExecutorOps::unknown_id, Status::not_found},
          {MockExecutorOps::undecodable_id, Status::malformed},
          {MockExecutorOps::lost_id, Status::invalid_state},
          /* A value the ABI does not define is a broken library, not a lost
           * device: folding the two together would have an operator chasing a
           * GPU fault that never happened (feature 042). */
          {MockExecutorOps::garbage_id, Status::not_supported},
      };
      for (const Case &c : cases)
          CHECK_STATUS(executor.execute(c.id, nullptr, 0u, nullptr, 0u, reply_size), c.status);
      executor.destroy();
  }); }

TEST(context_executor, each_sub_api_names_its_command_api)
{ Fixture fixture;
  fixture.executor().configure(SubApi::opencl, uuid, GlContextOptions());
  CHECK_EQ(fixture.executor().command_api(), 2u);
  fixture.executor().configure(SubApi::opengl, uuid, GlContextOptions());
  CHECK_EQ(fixture.executor().command_api(), 3u); }

TEST(context_executor, an_executor_left_behind_is_released_with_the_context)
{ MockHost host;
  MockExecutorOps ops(AV_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_STATUS(library.open_domain(uuid), Status::ok);
  {
      ExecutorShares  shares(host, library);
      ContextExecutor executor(host, library, shares, 1u);
      executor.configure(SubApi::vulkan, uuid, GlContextOptions());
      void *thread = nullptr;
      CHECK_STATUS(host.start_thread([](void *context) {
                       static_cast<void>(static_cast<ContextExecutor *>(context)->create());
                   },
                                     &executor, thread),
                   Status::ok);
      host.join_thread(thread);
      CHECK(executor.created());
  }
  CHECK_EQ(ops.destroyed(), 1u);
  CHECK_EQ(ops.misuses(), 1u);   /* released off the thread it was made on: the last resort */ }

TEST(context_executor, configures_on_the_main_loop_and_runs_on_a_worker)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  CHECK_STATUS(fixture.executor().create(), Status::ok);
  std::size_t reply_size = 0u;
  CHECK_STATUS(fixture.executor().execute(1u, nullptr, 0u, nullptr, 0u, reply_size), Status::ok);
  fixture.executor().destroy();
  for (int count = 0; count < 3; count++) CHECK(fixture.host().claim(HostViolation::worker_required));
  fixture.on_worker([](ContextExecutor &executor) { executor.configure(SubApi::vulkan, uuid, GlContextOptions()); });
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(context_executor_invariants, an_executor_without_a_configuration_is_detected)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.on_worker([](ContextExecutor &executor) { CHECK_STATUS(executor.create(), Status::ok); });
  av::ContextExecutorProbe::create_unconfigured(fixture.executor());
  CHECK(!fixture.executor().invariants_hold());
  fixture.on_worker([](ContextExecutor &executor) { executor.destroy(); }); }

namespace {

std::uint8_t page_bytes[8192];

AvBlobHandle host_pointer(std::uint32_t memory_type)
{
    AvBlobHandle handle = {};
    handle.kind = AV_BLOB_HOST_POINTER;
    handle.reserved = memory_type;
    handle.pointer = page_bytes;
    handle.fd = -1;
    handle.size = 4096u;
    return handle;
}

}  // namespace

/* Version 3's round trip: the import is made, then asked what it became, and
 * the answer is what the guest will name the memory by (feature 045). */
TEST(context_executor_import, imports_under_the_devices_id_and_answers_the_handle_the_library_named)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.on_worker([&fixture](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      std::uint64_t device_memory = 0u;
      CHECK_STATUS(executor.import(7u, host_pointer(3u), device_memory), Status::ok);
      CHECK_EQ(device_memory, 7u | MockExecutorOps::handle_bit);
      AvBlobHandle seen = {};
      CHECK(fixture.ops().import_of(7u, seen));
      CHECK(seen.pointer == page_bytes);
      CHECK_EQ(seen.reserved, 3u);
      CHECK_EQ(seen.size, 4096u);
      executor.release(7u);
      CHECK_EQ(fixture.ops().live_imports(), 0u);
      CHECK_EQ(fixture.ops().released(), 1u);
      executor.destroy();
      executor.release(7u);
      CHECK_STATUS(executor.import(8u, host_pointer(0u), device_memory), Status::invalid_state);
      CHECK_EQ(device_memory, 0u);
  });
  CHECK_EQ(fixture.ops().released(), 1u);
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(context_executor_import, a_refusal_or_an_import_named_nothing_leaves_nothing_imported)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.ops().refuse_import_at(1u);
  fixture.ops().withhold_handle_at(1u);
  fixture.on_worker([&fixture](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      std::uint64_t device_memory = 99u;
      CHECK_STATUS(executor.import(1u, host_pointer(0u), device_memory), Status::not_supported);
      CHECK_EQ(device_memory, 0u);
      CHECK_EQ(fixture.ops().imported(), 0u);
      CHECK_EQ(fixture.ops().released(), 0u);
      CHECK_STATUS(executor.import(2u, host_pointer(0u), device_memory), Status::not_supported);
      CHECK_EQ(device_memory, 0u);
      CHECK_EQ(fixture.ops().imported(), 1u);
      CHECK_EQ(fixture.ops().released(), 1u);   /* made, named nothing, and given back */
      CHECK_EQ(fixture.ops().live_imports(), 0u);
      executor.destroy();
  }); }

TEST(context_executor_import, runs_on_a_worker)
{ Fixture fixture;
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(fixture.executor().import(1u, host_pointer(0u), device_memory), Status::invalid_state);
  fixture.executor().release(1u);
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK(fixture.host().claim(HostViolation::worker_required)); }

/* ---- OpenGL options and sharing (feature 050) ---- */

namespace {

const GlContextOptions core_46(4u, 6u, GlContextOptions::Profile::core,
                               GlContextOptions::debug | GlContextOptions::lose_context_on_reset, 0u);

}  // namespace

/* An OpenGL executor is made with the context's options and published for
 * others to share while it lives; a Vulkan one is never published. */
TEST(context_executor_create, an_opengl_executor_is_made_with_its_options_and_published_while_it_lives)
{ Fixture fixture;
  fixture.executor().configure(SubApi::opengl, uuid, core_46);
  fixture.on_worker([&fixture](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      CHECK_EQ(fixture.shares().published(), 1u);
      executor.destroy();
      CHECK_EQ(fixture.shares().published(), 0u);
  });
  const AvContextOptions asked = fixture.ops().last_options();
  CHECK_EQ(asked.major, 4u);
  CHECK_EQ(asked.minor, 6u);
  CHECK_EQ(asked.profile, AV_CONTEXT_PROFILE_CORE);
  CHECK_EQ(asked.flags, AV_CONTEXT_FLAG_DEBUG | AV_CONTEXT_FLAG_LOSE_CONTEXT_ON_RESET);
  CHECK(asked.domain != 0u);
  CHECK(fixture.ops().last_share() == nullptr);
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.on_worker([&fixture](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      CHECK_EQ(fixture.shares().published(), 0u);
      executor.destroy();
  });
  /* The defaults ask for nothing. */
  CHECK_EQ(fixture.ops().last_options().major, 0u);
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(context_executor_create, a_share_context_is_the_published_executor_of_that_id)
{ Fixture fixture;
  fixture.executor().configure(SubApi::opengl, uuid, GlContextOptions());
  fixture.sharing().configure(SubApi::opengl, uuid, core_46.sharing(5u));
  struct Pair {
      ContextExecutor *first;
      ContextExecutor *second;
      Fixture         *fixture;
  } pair{&fixture.executor(), &fixture.sharing(), &fixture};
  void *thread = nullptr;
  CHECK_STATUS(fixture.host().start_thread([](void *context) {
                   const Pair &p = *static_cast<Pair *>(context);
                   CHECK_STATUS(p.first->create(), Status::ok);
                   CHECK_STATUS(p.second->create(), Status::ok);
                   CHECK(p.fixture->ops().last_share() != nullptr);
                   CHECK(p.fixture->ops().last_share_live());
                   CHECK_EQ(p.fixture->ops().last_options().major, 4u);
                   CHECK_EQ(p.fixture->shares().published(), 2u);
                   p.first->destroy();
                   p.second->destroy();
               },
                                           &pair, thread),
               Status::ok);
  fixture.host().join_thread(thread);
  CHECK_EQ(fixture.ops().live(), 0u);
  CHECK_EQ(fixture.shares().published(), 0u);
  CHECK_EQ(fixture.ops().misuses(), 0u); }

/* Nothing published under the share id is no executor, and nothing is asked. */
TEST(context_executor_create, a_share_context_with_no_executor_makes_none)
{ Fixture fixture;
  fixture.sharing().configure(SubApi::opengl, uuid, core_46.sharing(5u));
  fixture.on_worker([&fixture](ContextExecutor &) {
      CHECK_STATUS(fixture.sharing().create(), Status::no_memory);
  });
  CHECK(!fixture.sharing().created());
  CHECK_EQ(fixture.ops().option_creates(), 0u);
  CHECK_EQ(fixture.shares().published(), 0u); }

/* An OpenGL executor that cannot be published is given back: a context
 * another cannot share with has no way to say so later. */
TEST(context_executor_create, an_opengl_executor_that_cannot_be_published_is_given_back)
{ MockHost host;
  MockExecutorOps ops(AV_EXECUTOR_SUB_API_OPENGL);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  CHECK_STATUS(library.open_domain(uuid), Status::ok);
  ExecutorShares  unlocked(host, library);
  ContextExecutor executor(host, library, unlocked, 3u);
  executor.configure(SubApi::opengl, uuid, GlContextOptions());
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) {
                   CHECK_STATUS(static_cast<ContextExecutor *>(context)->create(), Status::invalid_state);
               },
                                 &executor, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(!executor.created());
  CHECK_EQ(ops.live(), 0u);
  CHECK_EQ(ops.misuses(), 0u); }

/* Feature 053: a context with no executor shares, opens and closes nothing,
 * and never calls into the library. */
TEST(context_executor_shared, refuses_without_an_executor)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.on_worker([](ContextExecutor &executor) {
      CHECK_STATUS(executor.share_object(0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), Status::invalid_state);
      std::uint64_t made = 9u;
      CHECK_STATUS(executor.open_object(0x2Au, AV_SHARED_MEMORY, 4096u, made), Status::invalid_state);
      CHECK_EQ(made, 0u);
      executor.close_object(0x2Au);
  });
  CHECK_EQ(fixture.ops().shared_objects(), 0u);
  CHECK_EQ(fixture.ops().closed_objects(), 0u); }

/* What the library answers is what the device tells the guest: done, an id
 * it does not share, one that disagrees, one already held, and a refusal. */
TEST(context_executor_shared, shares_opens_and_closes_through_the_library)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.sharing().configure(SubApi::vulkan, uuid, GlContextOptions());
  fixture.on_worker([&fixture](ContextExecutor &maker) {
      ContextExecutor &reader = fixture.sharing();
      CHECK_STATUS(maker.create(), Status::ok);
      CHECK_STATUS(reader.create(), Status::ok);
      CHECK_STATUS(maker.share_object(0x2Au, AV_SHARED_MEMORY, 4096u, 0x40u), Status::ok);
      std::uint64_t made = 0u;
      CHECK_STATUS(reader.open_object(0x2Au, AV_SHARED_MEMORY, 4096u, made), Status::ok);
      CHECK(made != 0u);
      CHECK_STATUS(reader.open_object(0x2Au, AV_SHARED_MEMORY, 4096u, made), Status::conflict);
      CHECK_STATUS(reader.open_object(0x2Bu, AV_SHARED_MEMORY, 4096u, made), Status::not_found);
      CHECK_STATUS(reader.open_object(0x2Au, AV_SHARED_MEMORY, 8192u, made), Status::invalid_parameter);
      fixture.ops().refuse_share_at(1u);
      CHECK_STATUS(maker.share_object(0x2Cu, AV_SHARED_MEMORY, 4096u, 0x41u), Status::not_supported);
      reader.close_object(0x2Au);
      CHECK_EQ(fixture.ops().closed_objects(), 1u);
      CHECK_EQ(fixture.ops().live_opens(), 0u);
      reader.destroy();
      maker.destroy();
  });
  CHECK_EQ(fixture.ops().shared_objects(), 1u);
  CHECK_EQ(fixture.ops().opened_objects(), 1u);
  CHECK_EQ(fixture.ops().misuses(), 0u); }
