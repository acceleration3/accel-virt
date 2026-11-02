#include "core/support/status.hpp"
#include "device/execution/batch_staging.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/host/host_violation.hpp"
#include "harness/host/mock_host.hpp"

#include <cstdint>

using av::BatchStaging;
using av::HostViolation;
using av::MockHost;
using av::Status;

namespace av {

struct BatchStagingProbe {
    static void lose_reply(BatchStaging &staging) { staging.reply_capacity_ = 8u; }
    static void lose_payload(BatchStaging &staging) { staging.payload_capacity_ = 8u; }
    static void repair(BatchStaging &staging) { staging.payload_capacity_ = 0u; staging.reply_capacity_ = 0u; }
};

}

TEST(batch_staging_allocate, holds_a_payload_block_and_a_reply_block_until_released)
{ MockHost host;
  BatchStaging staging(host);
  CHECK(staging.payload() == nullptr);
  CHECK(staging.invariants_hold());
  CHECK_STATUS(staging.allocate(4096u, 100u), Status::ok);
  CHECK(staging.invariants_hold());
  CHECK(staging.payload() != nullptr && staging.reply() != nullptr);
  CHECK_EQ(staging.payload_capacity(), 4096u);
  CHECK_EQ(staging.reply_capacity(), 100u);
  staging.payload()[4095] = 1u;
  staging.reply()[99] = 1u;
  CHECK_EQ(host.live_allocations(), 2u);
  CHECK_STATUS(staging.allocate(4096u, 100u), Status::invalid_state);
  staging.release();
  CHECK(staging.invariants_hold());
  CHECK_EQ(host.live_allocations(), 0u);
  CHECK_EQ(staging.reply_capacity(), 0u);
  staging.release(); }

TEST(batch_staging_allocate, no_reply_slot_takes_no_reply_block_and_a_huge_one_is_capped)
{ MockHost host;
  BatchStaging none(host);
  CHECK_STATUS(none.allocate(4096u, 0u), Status::ok);
  CHECK(none.reply() == nullptr);
  CHECK_EQ(host.live_allocations(), 1u);
  BatchStaging huge(host);
  CHECK_STATUS(huge.allocate(4096u, BatchStaging::max_reply + 1u), Status::ok);
  CHECK_EQ(huge.reply_capacity(), BatchStaging::max_reply); }

TEST(batch_staging_allocate, a_host_out_of_memory_leaves_nothing_held)
{ for (std::uint32_t failing = 1u; failing <= 2u; failing++) {
      MockHost host;
      BatchStaging staging(host);
      host.fail_allocation_at(failing);
      CHECK_STATUS(staging.allocate(4096u, 64u), Status::no_memory);
      CHECK_EQ(host.live_allocations(), 0u);
      CHECK(staging.payload() == nullptr);
      CHECK(staging.invariants_hold());
  } }

TEST(batch_staging, allocates_on_the_main_loop)
{ MockHost host;
  BatchStaging staging(host);
  void *thread = nullptr;
  CHECK_STATUS(host.start_thread([](void *context) {
                   auto *const s = static_cast<BatchStaging *>(context);
                   static_cast<void>(s->allocate(16u, 0u));
                   s->release();
               },
                                 &staging, thread),
               Status::ok);
  host.join_thread(thread);
  CHECK(host.claim(HostViolation::main_loop_required));
  CHECK(host.claim(HostViolation::main_loop_required)); }

TEST(batch_staging_invariants, a_capacity_without_its_block_is_detected)
{ MockHost host;
  BatchStaging staging(host);
  av::BatchStagingProbe::lose_reply(staging);
  CHECK(!staging.invariants_hold());
  av::BatchStagingProbe::repair(staging);
  av::BatchStagingProbe::lose_payload(staging);
  CHECK(!staging.invariants_hold());
  av::BatchStagingProbe::repair(staging); }
