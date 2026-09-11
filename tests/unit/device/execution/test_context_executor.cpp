/* One context's host executor: made and released on the context's worker,
 * running commands there, and every answer a library gives read as a status. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/execution/context_executor.hpp"
#include "device/execution/executor_library.hpp"
#include "harness/executor/mock_executor_ops.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"
#include "proto/context/sub_api.hpp"

#include <cstdint>
#include <cstring>

using av::ContextExecutor;
using av::ExecutorLibrary;
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
    Fixture() : ops_(RVK_EXECUTOR_SUB_API_VULKAN), library_(), executor_(host_, library_)
    {
        static_cast<void>(library_.accept(&ops_.table()));
    }

    MockHost        &host() { return host_; }
    MockExecutorOps &ops() { return ops_; }
    ContextExecutor &executor() { return executor_; }

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
    ContextExecutor executor_;
};

}  // namespace

TEST(context_executor_create, makes_the_configured_executor_on_the_worker_and_releases_it_there)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid);
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
  CHECK_EQ(fixture.ops().last_sub_api(), RVK_EXECUTOR_SUB_API_VULKAN);
  CHECK_EQ(fixture.ops().last_uuid_byte(), 0x5Au);
  CHECK_EQ(fixture.ops().misuses(), 0u); }

TEST(context_executor_create, an_unconfigured_context_or_a_library_that_makes_nothing_has_no_executor)
{ Fixture fixture;
  fixture.on_worker([](ContextExecutor &executor) { CHECK_STATUS(executor.create(), Status::invalid_state); });
  fixture.executor().configure(SubApi::vulkan, uuid);
  fixture.ops().fail_create_at(1u);
  fixture.on_worker([](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::no_memory);
      std::size_t reply_size = 0u;
      CHECK_STATUS(executor.execute(0x10001u, nullptr, 0u, nullptr, 0u, reply_size), Status::invalid_state);
  });
  CHECK(!fixture.executor().created()); }

TEST(context_executor_execute, reads_every_answer_a_library_gives)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid);
  fixture.on_worker([](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      std::size_t reply_size = 0u;
      struct Case { std::uint32_t id; Status status; };
      const Case cases[] = {
          {MockExecutorOps::unknown_id, Status::not_found},
          {MockExecutorOps::undecodable_id, Status::malformed},
          {MockExecutorOps::lost_id, Status::invalid_state},
          {MockExecutorOps::garbage_id, Status::invalid_state},
      };
      for (const Case &c : cases)
          CHECK_STATUS(executor.execute(c.id, nullptr, 0u, nullptr, 0u, reply_size), c.status);
      executor.destroy();
  }); }

TEST(context_executor, each_sub_api_names_its_command_api)
{ Fixture fixture;
  fixture.executor().configure(SubApi::opencl, uuid);
  CHECK_EQ(fixture.executor().command_api(), 2u);
  fixture.executor().configure(SubApi::opengl, uuid);
  CHECK_EQ(fixture.executor().command_api(), 3u); }

TEST(context_executor, an_executor_left_behind_is_released_with_the_context)
{ MockHost host;
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  {
      ContextExecutor executor(host, library);
      executor.configure(SubApi::vulkan, uuid);
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
  fixture.executor().configure(SubApi::vulkan, uuid);
  CHECK_STATUS(fixture.executor().create(), Status::ok);
  std::size_t reply_size = 0u;
  CHECK_STATUS(fixture.executor().execute(1u, nullptr, 0u, nullptr, 0u, reply_size), Status::ok);
  fixture.executor().destroy();
  for (int count = 0; count < 3; count++) CHECK(fixture.host().claim(HostViolation::worker_required));
  fixture.on_worker([](ContextExecutor &executor) { executor.configure(SubApi::vulkan, uuid); });
  CHECK(fixture.host().claim(HostViolation::main_loop_required)); }

TEST(context_executor_invariants, an_executor_without_a_configuration_is_detected)
{ Fixture fixture;
  fixture.executor().configure(SubApi::vulkan, uuid);
  fixture.on_worker([](ContextExecutor &executor) { CHECK_STATUS(executor.create(), Status::ok); });
  av::ContextExecutorProbe::create_unconfigured(fixture.executor());
  CHECK(!fixture.executor().invariants_hold());
  fixture.on_worker([](ContextExecutor &executor) { executor.destroy(); }); }
