#include "executor/host_ram_ledger.hpp"
#include "executor/proc_memory_gauge.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ProcMemoryGauge;

TEST(proc_memory_gauge, the_host_answers_its_available_memory_from_a_file_kept_open_across_reads)
{ ProcMemoryGauge gauge;
  std::uint64_t   first = 0u;
  std::uint64_t   second = 0u;
  CHECK(gauge.available(first));
  CHECK(gauge.available(second));
  CHECK(first > 1ull << 20);
  CHECK(second > 1ull << 20); }

TEST(proc_memory_gauge, the_clock_never_runs_backwards)
{ ProcMemoryGauge gauge;
  const std::uint64_t before = gauge.now_ms();
  CHECK(gauge.now_ms() >= before); }

TEST(proc_memory_gauge, a_floor_above_everything_the_host_has_refuses_and_a_floor_of_one_page_does_not)
{ ProcMemoryGauge gauge;
  av::HostRamLedger ledger;
  ledger.watch(&gauge);
  ledger.floor(1ull << 62);
  CHECK(!ledger.take(4096u));
  av::HostRamLedger roomy;
  roomy.watch(&gauge);
  roomy.floor(4096u);
  CHECK(roomy.take(4096u)); }
