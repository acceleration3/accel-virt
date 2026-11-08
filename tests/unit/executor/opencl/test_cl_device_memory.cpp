#include "executor/opencl/cl_device_memory.hpp"
#include "executor/host_ram_ledger.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/failing_blocks.hpp"

#include <CL/cl.h>

#include <cstdint>

using av::ClDeviceMemory;
using av::VramLedger;

namespace {

constexpr std::uint64_t gib = 1ull << 30;

cl_mem handle(std::uintptr_t value) { return reinterpret_cast<cl_mem>(value); }

struct Rig {
    Rig() : blocks(), ledger(), memory(blocks)
    { ledger.learn(16 * gib);
      memory.bind(&ledger, nullptr); }

    av::FailingBlocks blocks;
    VramLedger        ledger;
    ClDeviceMemory    memory;
};

}

TEST(cl_device_memory, a_claim_that_fits_is_held_until_its_object_is_forgotten)
{ Rig rig;
  std::uint64_t claimed = 0u;
  CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, 4 * gib, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, 4 * gib);
  CHECK(rig.memory.keep(handle(0x10), claimed));
  CHECK_EQ(rig.memory.held(), 4 * gib);
  CHECK_EQ(rig.ledger.used(), 4 * gib);
  rig.memory.forget(handle(0x10));
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_EQ(rig.memory.objects(), 0u);
  CHECK(rig.memory.invariants_hold()); }

TEST(cl_device_memory, a_claim_past_the_cap_fails_with_the_allocation_failure_and_holds_nothing)
{ Rig rig;
  std::uint64_t claimed = 7u;
  CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, 15 * gib, claimed), CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK_EQ(claimed, 0u);
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_EQ(rig.ledger.refusals(), 1u); }

TEST(cl_device_memory, memory_the_host_keeps_in_ram_is_not_counted_but_a_copy_into_the_card_is)
{ Rig rig;
  std::uint64_t claimed = 7u;
  CHECK_EQ(rig.memory.claim(CL_MEM_USE_HOST_PTR, 20 * gib, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, 0u);
  CHECK_EQ(rig.memory.claim(CL_MEM_ALLOC_HOST_PTR, 20 * gib, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, 0u);
  CHECK_EQ(rig.memory.claim(CL_MEM_COPY_HOST_PTR, gib, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, gib);
  CHECK(rig.memory.keep(handle(0x20), claimed));
  CHECK(rig.memory.keep(handle(0x30), 0u));
  CHECK_EQ(rig.memory.objects(), 1u); }

TEST(cl_device_memory, a_zero_byte_claim_takes_nothing)
{ Rig rig;
  std::uint64_t claimed = 7u;
  CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, 0u, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, 0u); }

TEST(cl_device_memory, a_failed_host_create_gives_the_claim_back)
{ Rig rig;
  std::uint64_t claimed = 0u;
  CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, 3 * gib, claimed), CL_SUCCESS);
  rig.memory.unclaim(claimed);
  CHECK_EQ(rig.ledger.used(), 0u); }

TEST(cl_device_memory, an_object_that_cannot_be_recorded_returns_its_claim)
{ Rig rig;
  std::uint64_t claimed = 0u;
  CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, gib, claimed), CL_SUCCESS);
  rig.blocks.fail_from(rig.blocks.requests() + 1u);
  CHECK(!rig.memory.keep(handle(0x40), claimed));
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK(rig.memory.invariants_hold()); }

TEST(cl_device_memory, forgetting_an_unknown_object_changes_nothing)
{ Rig rig;
  std::uint64_t claimed = 0u;
  CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, gib, claimed), CL_SUCCESS);
  CHECK(rig.memory.keep(handle(0x50), claimed));
  CHECK_EQ(rig.memory.forget(handle(0x60)), 0u);
  CHECK_EQ(rig.memory.forget(handle(0x50)), gib);
  CHECK_EQ(rig.memory.forget(handle(0x50)), 0u);
  CHECK_EQ(rig.ledger.used(), 0u); }

TEST(cl_device_memory, clearing_returns_every_object_the_guest_never_released)
{ Rig rig;
  for (std::uintptr_t key = 1u; key <= 5u; key++) {
      std::uint64_t claimed = 0u;
      CHECK_EQ(rig.memory.claim(CL_MEM_READ_WRITE, gib, claimed), CL_SUCCESS);
      CHECK(rig.memory.keep(handle(key * 16u), claimed));
  }
  CHECK_EQ(rig.ledger.used(), 5 * gib);
  CHECK_EQ(rig.memory.clear(), 5 * gib);
  CHECK_EQ(rig.ledger.used(), 0u);
  CHECK_EQ(rig.memory.objects(), 0u); }

TEST(cl_device_memory, with_no_ledger_bound_nothing_is_ever_refused)
{ av::FailingBlocks blocks;
  ClDeviceMemory memory(blocks);
  std::uint64_t claimed = 7u;
  CHECK_EQ(memory.claim(CL_MEM_READ_WRITE, 1000 * gib, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, 0u);
  CHECK_EQ(memory.cap(), UINT64_MAX); }

namespace {

struct HostRig {
    HostRig() : rig(), host()
    { host.budget(8 * gib);
      rig.memory.bind(&rig.ledger, &host); }

    Rig               rig;
    av::HostRamLedger host;
};

}

TEST(cl_device_memory_host_ram, memory_the_driver_keeps_in_ram_is_counted_against_the_vms_host_ram_and_not_the_card)
{ HostRig fixture;
  std::uint64_t claimed = 0u;
  CHECK_EQ(fixture.rig.memory.claim(CL_MEM_ALLOC_HOST_PTR, 5 * gib, claimed), CL_SUCCESS);
  CHECK(claimed != 0u);
  CHECK(fixture.rig.memory.keep(handle(0x40), claimed));
  CHECK_EQ(fixture.host.used(), 5 * gib);
  CHECK_EQ(fixture.rig.ledger.used(), 0u);
  CHECK_EQ(fixture.rig.memory.host_held(), 5 * gib);
  CHECK_EQ(fixture.rig.memory.held(), 0u);
  CHECK(fixture.rig.memory.invariants_hold());
  fixture.rig.memory.forget(handle(0x40));
  CHECK_EQ(fixture.host.used(), 0u); }

TEST(cl_device_memory_host_ram, a_host_claim_past_the_budget_is_out_of_host_memory_and_holds_nothing)
{ HostRig fixture;
  std::uint64_t claimed = 7u;
  CHECK_EQ(fixture.rig.memory.claim(CL_MEM_USE_HOST_PTR, 5 * gib, claimed), CL_SUCCESS);
  CHECK(fixture.rig.memory.keep(handle(0x40), claimed));
  claimed = 7u;
  CHECK_EQ(fixture.rig.memory.claim(CL_MEM_ALLOC_HOST_PTR, 4 * gib, claimed), CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(claimed, 0u);
  CHECK_EQ(fixture.host.used(), 5 * gib);
  CHECK_EQ(fixture.host.refusals(), 1u); }

TEST(cl_device_memory_host_ram, a_card_claim_still_fails_with_the_allocation_failure_while_host_ram_has_room)
{ HostRig fixture;
  std::uint64_t claimed = 0u;
  CHECK_EQ(fixture.rig.memory.claim(CL_MEM_READ_WRITE, 15 * gib, claimed), CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK_EQ(fixture.host.used(), 0u); }

TEST(cl_device_memory_host_ram, an_unclaim_and_a_clear_return_host_claims_to_the_host_ledger)
{ HostRig fixture;
  std::uint64_t first = 0u;
  std::uint64_t second = 0u;
  CHECK_EQ(fixture.rig.memory.claim(CL_MEM_ALLOC_HOST_PTR, 2 * gib, first), CL_SUCCESS);
  fixture.rig.memory.unclaim(first);
  CHECK_EQ(fixture.host.used(), 0u);
  CHECK_EQ(fixture.rig.memory.claim(CL_MEM_ALLOC_HOST_PTR, 3 * gib, second), CL_SUCCESS);
  CHECK(fixture.rig.memory.keep(handle(0x50), second));
  fixture.rig.memory.clear();
  CHECK_EQ(fixture.host.used(), 0u);
  CHECK_EQ(fixture.rig.memory.objects(), 0u); }

TEST(cl_device_memory_host_ram, without_a_host_ledger_host_resident_memory_stays_uncounted)
{ Rig rig;
  std::uint64_t claimed = 7u;
  CHECK_EQ(rig.memory.claim(CL_MEM_ALLOC_HOST_PTR, 40 * gib, claimed), CL_SUCCESS);
  CHECK_EQ(claimed, 0u); }
