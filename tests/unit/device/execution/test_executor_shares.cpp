#include "core/support/status.hpp"
#include "device/abi/av_executor.h"
#include "device/execution/executor_library.hpp"
#include "device/execution/executor_shares.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/sub_api.hpp"

#include <atomic>
#include <cstdint>
#include <sched.h>

using av::ExecutorLibrary;
using av::ExecutorShares;
using av::MockExecutorOps;
using av::MockHost;
using av::Status;
using av::SubApi;

namespace av {

struct ExecutorSharesProbe {
    static void twin(ExecutorShares &shares)
    {
        shares.ids_[1] = shares.ids_[0];
        shares.executors_[1] = shares.executors_[0];
    }
    static void orphan(ExecutorShares &shares, std::uint32_t id) { shares.ids_[2] = id; }
};

}

namespace {

constexpr std::uint8_t gpu[16] = {0x11};
constexpr std::uint8_t other_gpu[16] = {0x22};
const AvContextOptions core_46 = {4u, 6u, AV_CONTEXT_PROFILE_CORE, 0u, 0u};

class Fixture {
public:
    Fixture() : ops_(AV_EXECUTOR_SUB_API_OPENGL), shares_(host_, library_)
    {
        static_cast<void>(library_.accept(&ops_.table()));
        static_cast<void>(library_.open_domain(gpu));
        CHECK_STATUS(shares_.initialize(), Status::ok);
    }

    MockHost        &host() { return host_; }
    MockExecutorOps &ops() { return ops_; }
    ExecutorLibrary &library() { return library_; }
    ExecutorShares  &shares() { return shares_; }

    template <typename Body>
    void on_worker(Body body)
    {
        void *const thread = start(body);
        host_.join_thread(thread);
    }
    template <typename Body>
    void *start(Body &body)
    {
        void *thread = nullptr;
        CHECK_STATUS(host_.start_thread([](void *context) { (*static_cast<Body *>(context))(); }, &body, thread),
                     Status::ok);
        return thread;
    }

private:
    MockHost        host_;
    MockExecutorOps ops_;
    ExecutorLibrary library_;
    ExecutorShares  shares_;
};

}

TEST(executor_shares, a_published_executor_is_shared_by_a_create_on_the_same_gpu)
{ Fixture fixture;
  ExecutorShares &shares = fixture.shares();
  fixture.on_worker([&] {
      AvExecutor *const first = fixture.library().create(SubApi::opengl, AvContextOptions{}, nullptr);
      CHECK_STATUS(shares.publish(3u, gpu, first), Status::ok);
      CHECK_EQ(shares.published(), 1u);
      CHECK(shares.invariants_hold());
      AvExecutor *const second = shares.create_sharing(av::SubApi::opengl, 3u, gpu, core_46);
      CHECK(second != nullptr);
      CHECK(fixture.ops().last_share() == first);
      CHECK(fixture.ops().last_share_live());
      CHECK_EQ(fixture.ops().last_options().major, 4u);
      CHECK_EQ(fixture.ops().last_sub_api(), AV_EXECUTOR_SUB_API_OPENGL);
      shares.withdraw(3u);
      CHECK_EQ(shares.published(), 0u);
      CHECK(shares.invariants_hold());
      fixture.library().destroy(second);
      fixture.library().destroy(first);
  });
  CHECK_EQ(fixture.ops().misuses(), 0u);
  CHECK_EQ(fixture.ops().live(), 0u); }

TEST(executor_shares, nothing_published_on_that_gpu_is_no_executor_and_no_call)
{ Fixture fixture;
  ExecutorShares &shares = fixture.shares();
  fixture.on_worker([&] {
      AvExecutor *const first = fixture.library().create(SubApi::opengl, AvContextOptions{}, nullptr);
      const std::uint32_t asked = fixture.ops().option_creates();
      CHECK(shares.create_sharing(av::SubApi::opengl, 3u, gpu, core_46) == nullptr);
      CHECK_STATUS(shares.publish(3u, gpu, first), Status::ok);
      CHECK(shares.create_sharing(av::SubApi::opengl, 4u, gpu, core_46) == nullptr);
      CHECK(shares.create_sharing(av::SubApi::opengl, 0u, gpu, core_46) == nullptr);
      CHECK(shares.create_sharing(av::SubApi::opengl, 3u, other_gpu, core_46) == nullptr);
      shares.withdraw(3u);
      shares.withdraw(3u);
      shares.withdraw(9u);
      CHECK(shares.create_sharing(av::SubApi::opengl, 3u, gpu, core_46) == nullptr);
      CHECK_EQ(fixture.ops().option_creates(), asked);
      CHECK_STATUS(shares.publish(3u, gpu, first), Status::ok);
      fixture.ops().fail_create_at(1u);
      CHECK(shares.create_sharing(av::SubApi::opengl, 3u, gpu, core_46) == nullptr);
      CHECK_EQ(fixture.ops().option_creates(), asked + 1u);
      shares.withdraw(3u);
      fixture.library().destroy(first);
  });
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(executor_shares, a_withdraw_waits_for_a_create_that_is_using_the_executor)
{ Fixture fixture;
  ExecutorShares &shares = fixture.shares();
  AvExecutor *shared = nullptr;
  std::atomic<bool> published{false};
  std::atomic<bool> withdrawing{false};
  std::atomic<bool> withdrawn{false};
  std::atomic<bool> destroy_now{false};
  auto owner = [&] {
      shared = fixture.library().create(SubApi::opengl, AvContextOptions{}, nullptr);
      CHECK_STATUS(shares.publish(5u, gpu, shared), Status::ok);
      published = true;
      while (!destroy_now) sched_yield();
      withdrawing = true;
      shares.withdraw(5u);
      withdrawn = true;
      fixture.library().destroy(shared);
  };
  AvExecutor *made = nullptr;
  auto sharer = [&] { made = shares.create_sharing(av::SubApi::opengl, 5u, gpu, core_46); };
  void *const owning = fixture.start(owner);
  while (!published) sched_yield();
  fixture.ops().hold_creates();
  void *const sharing = fixture.start(sharer);
  while (fixture.ops().creates_waiting() == 0u) sched_yield();
  destroy_now = true;
  while (!withdrawing) sched_yield();
  for (int spins = 0; spins < 2000; spins++) sched_yield();
  CHECK(!withdrawn);
  fixture.ops().release_creates();
  fixture.host().join_thread(sharing);
  fixture.host().join_thread(owning);
  CHECK(withdrawn);
  CHECK(made != nullptr);
  CHECK(fixture.ops().last_share() == shared);
  CHECK(fixture.ops().last_share_live());
  CHECK_EQ(shares.published(), 0u);
  fixture.on_worker([&] { fixture.library().destroy(made); });
  CHECK_EQ(fixture.ops().live(), 0u); }

TEST(executor_shares, publishing_needs_the_lock_and_a_free_entry)
{ MockHost host;
  MockExecutorOps ops(AV_EXECUTOR_SUB_API_OPENGL);
  ExecutorLibrary library;
  static_cast<void>(library.accept(&ops.table()));
  ExecutorShares shares(host, library);
  AvExecutor *const fake = static_cast<AvExecutor *>(static_cast<void *>(&host));
  CHECK_STATUS(shares.publish(1u, gpu, fake), Status::invalid_state);
  CHECK(shares.create_sharing(av::SubApi::opengl, 1u, gpu, core_46) == nullptr);
  shares.withdraw(1u);
  CHECK_EQ(shares.published(), 0u);
  CHECK_EQ(ops.option_creates(), 0u);
  host.fail_mutex_at(1u);
  CHECK_STATUS(shares.initialize(), Status::no_memory);
  CHECK_STATUS(shares.initialize(), Status::ok);
  CHECK_STATUS(shares.initialize(), Status::invalid_state);
  CHECK_STATUS(shares.publish(0u, gpu, fake), Status::invalid_parameter);
  CHECK_STATUS(shares.publish(1u, gpu, nullptr), Status::invalid_parameter);
  for (std::uint32_t id = 1u; id <= ExecutorShares::capacity; id++)
      CHECK_STATUS(shares.publish(id, gpu, fake), Status::ok);
  CHECK_STATUS(shares.publish(ExecutorShares::capacity + 1u, gpu, fake), Status::no_space);
  CHECK_STATUS(shares.publish(1u, gpu, fake), Status::conflict);
  CHECK_EQ(shares.published(), ExecutorShares::capacity);
  CHECK(shares.invariants_hold());
  shares.withdraw(7u);
  CHECK_STATUS(shares.publish(ExecutorShares::capacity + 1u, gpu, fake), Status::ok);
  CHECK(shares.invariants_hold()); }

TEST(executor_shares, invariants_refuse_an_id_twice_or_an_entry_without_an_executor)
{ MockHost host;
  ExecutorLibrary library;
  ExecutorShares shares(host, library);
  CHECK_STATUS(shares.initialize(), Status::ok);
  AvExecutor *const fake = static_cast<AvExecutor *>(static_cast<void *>(&host));
  CHECK_STATUS(shares.publish(1u, gpu, fake), Status::ok);
  CHECK(shares.invariants_hold());
  av::ExecutorSharesProbe::twin(shares);
  CHECK(!shares.invariants_hold());
  ExecutorShares orphaned(host, library);
  CHECK_STATUS(orphaned.initialize(), Status::ok);
  av::ExecutorSharesProbe::orphan(orphaned, 4u);
  CHECK(!orphaned.invariants_hold()); }
