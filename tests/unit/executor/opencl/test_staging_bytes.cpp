#include "executor/host_ram_ledger.hpp"
#include "harness/executor/fake_memory_gauge.hpp"
#include "executor/opencl/staging_bytes.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::StagingBytes;

TEST(staging_bytes, an_ask_is_rounded_to_pages_and_a_smaller_one_keeps_the_memory)
{ StagingBytes staging;
  CHECK(staging.bytes() == nullptr);
  CHECK_EQ(staging.held(), 0u);
  CHECK(staging.reserve(0u));
  CHECK(staging.bytes() == nullptr);
  CHECK(staging.reserve(1u));
  CHECK_EQ(staging.held(), 4096u);
  std::uint8_t *const first = staging.bytes();
  CHECK(first != nullptr);
  first[4095] = 0x5Au;
  CHECK(staging.reserve(4096u));
  CHECK(staging.bytes() == first);
  CHECK_EQ(staging.bytes()[4095], 0x5Au);
  CHECK(staging.reserve(4097u));
  CHECK_EQ(staging.held(), 8192u);
  staging.bytes()[8191] = 1u;
  CHECK_EQ(staging.bytes()[8191], 1u); }

TEST(staging_bytes, a_size_that_cannot_be_had_is_refused_and_what_was_held_stays)
{ StagingBytes staging;
  CHECK(staging.reserve(100u));
  std::uint8_t *const held = staging.bytes();
  CHECK(!staging.reserve(SIZE_MAX));
  CHECK(!staging.reserve(SIZE_MAX - 4095u));
  CHECK(staging.bytes() == held);
  CHECK_EQ(staging.held(), 4096u);
  CHECK(!staging.reserve(SIZE_MAX - 8192u));
  CHECK(staging.bytes() == held);
  CHECK_EQ(staging.held(), 4096u); }

TEST(staging_bytes_host_ram, the_pages_held_are_charged_to_the_vms_host_ram_and_a_refusal_keeps_what_was_held)
{ av::HostRamLedger ledger;
  ledger.budget(3u * 4096u);
  StagingBytes staging;
  staging.bind(&ledger);
  CHECK(staging.reserve(4096u));
  CHECK_EQ(ledger.used(), 4096u);
  std::uint8_t *const held = staging.bytes();
  CHECK(!staging.reserve(3u * 4096u));
  CHECK(staging.bytes() == held);
  CHECK_EQ(ledger.used(), 4096u);
  CHECK(ledger.refusals() != 0u);
  CHECK(staging.reserve(2u * 4096u));
  CHECK_EQ(ledger.used(), 2u * 4096u);
  CHECK_EQ(staging.held(), 2u * 4096u); }

TEST(staging_bytes_host_ram, releasing_and_destroying_give_the_pages_back_and_a_new_reserve_maps_again)
{ av::HostRamLedger ledger;
  {
    StagingBytes staging;
    staging.bind(&ledger);
    CHECK(staging.reserve(5000u));
    CHECK_EQ(ledger.used(), 8192u);
    staging.release();
    CHECK_EQ(ledger.used(), 0u);
    CHECK(staging.bytes() == nullptr);
    CHECK_EQ(staging.held(), 0u);
    CHECK(staging.reserve(100u));
    CHECK_EQ(ledger.used(), 4096u);
  }
  CHECK_EQ(ledger.used(), 0u); }

TEST(staging_bytes_host_ram, binding_to_another_ledger_gives_the_old_one_its_pages_back)
{ av::HostRamLedger first;
  av::HostRamLedger second;
  StagingBytes staging;
  staging.bind(&first);
  CHECK(staging.reserve(4096u));
  staging.bind(&second);
  CHECK_EQ(first.used(), 0u);
  CHECK_EQ(second.used(), 0u);
  CHECK(staging.reserve(4096u));
  CHECK_EQ(second.used(), 4096u); }

TEST(staging_bytes_host_ram, an_unbound_staging_is_not_limited_and_releasing_nothing_is_harmless)
{ StagingBytes staging;
  staging.release();
  CHECK(staging.reserve(1u << 20));
  staging.release(); }

TEST(staging_bytes_host_ram, a_reserve_the_host_cannot_spare_fails_and_keeps_what_was_held)
{ av::HostRamLedger ledger;
  av::FakeMemoryGauge gauge;
  gauge.free_bytes = 5u * 4096u;
  ledger.watch(&gauge);
  ledger.floor(4u * 4096u);
  StagingBytes staging;
  staging.bind(&ledger);
  CHECK(staging.reserve(4096u));
  std::uint8_t *const held = staging.bytes();
  CHECK(!staging.reserve(3u * 4096u));
  CHECK(staging.bytes() == held);
  CHECK_EQ(staging.held(), 4096u);
  CHECK(ledger.tight_refusals() != 0u); }
