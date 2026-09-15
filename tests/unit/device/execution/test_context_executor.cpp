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

namespace {

std::uint8_t page_bytes[8192];

RvkBlobHandle host_pointer(std::uint32_t memory_type)
{
    RvkBlobHandle handle = {};
    handle.kind = RVK_BLOB_HOST_POINTER;
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
  fixture.executor().configure(SubApi::vulkan, uuid);
  fixture.on_worker([&fixture](ContextExecutor &executor) {
      CHECK_STATUS(executor.create(), Status::ok);
      std::uint64_t device_memory = 0u;
      CHECK_STATUS(executor.import(7u, host_pointer(3u), device_memory), Status::ok);
      CHECK_EQ(device_memory, 7u | MockExecutorOps::handle_bit);
      RvkBlobHandle seen = {};
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
  fixture.executor().configure(SubApi::vulkan, uuid);
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

/* A version 2 library imports, but nothing it imports could be named: the
 * device does not ask it to make an import no guest command can reach. */
TEST(context_executor_import, a_library_before_version_3_is_not_asked)
{ MockHost host;
  MockExecutorOps ops(RVK_EXECUTOR_SUB_API_VULKAN);
  ops.editable().abi_version = RVK_EXECUTOR_ABI_VERSION_IMPORTS;
  ops.editable().imported_handle = nullptr;
  ExecutorLibrary library;
  CHECK_STATUS(library.accept(&ops.table()), Status::ok);
  ContextExecutor executor(host, library);
  executor.configure(SubApi::vulkan, uuid);
  struct Call { ContextExecutor *executor; Status status; std::uint64_t device_memory; };
  Call call{&executor, Status::ok, 5u};
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) {
                   auto *const c = static_cast<Call *>(context);
                   static_cast<void>(c->executor->create());
                   c->status = c->executor->import(1u, host_pointer(0u), c->device_memory);
                   c->executor->destroy();
               }, &call, thread), Status::ok);
  host.join_thread(thread);
  CHECK_STATUS(call.status, Status::not_supported);
  CHECK_EQ(call.device_memory, 0u);
  CHECK_EQ(ops.imported(), 0u); }

TEST(context_executor_import, runs_on_a_worker)
{ Fixture fixture;
  std::uint64_t device_memory = 0u;
  CHECK_STATUS(fixture.executor().import(1u, host_pointer(0u), device_memory), Status::invalid_state);
  fixture.executor().release(1u);
  CHECK(fixture.host().claim(HostViolation::worker_required));
  CHECK(fixture.host().claim(HostViolation::worker_required)); }
