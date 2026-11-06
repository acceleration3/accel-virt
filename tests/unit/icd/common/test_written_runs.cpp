#include "harness/framework/test_registry.hpp"
#include "icd/common/written_runs.hpp"

#include <cstdint>

using av::icd::WrittenRuns;

namespace av::icd {
struct WrittenRunsProbe {
    static void set(WrittenRuns &runs, std::uint32_t row, std::uint64_t offset, std::uint64_t bytes)
    {
        runs.runs_[row] = WrittenRuns::Run{offset, bytes};
    }
    static void set_count(WrittenRuns &runs, std::uint32_t count)
    {
        runs.count_ = count;
    }
};

}

using av::icd::WrittenRunsProbe;

namespace {

constexpr std::uint64_t gap = WrittenRuns::merge_gap;

}

TEST(written_runs, empty_and_overflowing_spans_are_accepted_and_kept_out)
{ WrittenRuns runs;
  CHECK(runs.invariants_hold());
  CHECK(runs.note(10u, 0u));
  CHECK(runs.note(~std::uint64_t{0} - 3u, 4u + 1u));
  CHECK_EQ(runs.count(), 0u);
  CHECK(runs.note(~std::uint64_t{0} - 4u, 4u));
  CHECK_EQ(runs.count(), 1u);
  CHECK(runs.invariants_hold()); }

TEST(written_runs, a_span_before_the_end_of_the_last_run_is_refused)
{ WrittenRuns runs;
  CHECK(runs.note(1000u, 100u));
  CHECK(!runs.note(1050u, 10u));
  CHECK(!runs.note(0u, 4u));
  CHECK_EQ(runs.count(), 1u);
  CHECK_EQ(runs.run(0u).offset_, 1000u);
  CHECK_EQ(runs.run(0u).bytes_, 100u);
  CHECK(runs.invariants_hold()); }

TEST(written_runs, near_spans_merge_and_far_spans_start_a_run)
{ WrittenRuns runs;
  CHECK(runs.note(0u, 10u));
  CHECK(runs.note(10u + gap - 1u, 5u));
  CHECK_EQ(runs.count(), 1u);
  CHECK_EQ(runs.run(0u).bytes_, 10u + gap + 4u);
  CHECK(runs.note(10u + gap + 4u + gap, 5u));
  CHECK_EQ(runs.count(), 2u);
  CHECK_EQ(runs.run(1u).offset_, 10u + gap + 4u + gap);
  CHECK_EQ(runs.run(2u).offset_, 0u);
  CHECK_EQ(runs.run(2u).bytes_, 0u);
  CHECK(runs.invariants_hold());
  runs.clear();
  CHECK_EQ(runs.count(), 0u); }

TEST(written_runs, a_full_table_refuses_another_distinct_run)
{ WrittenRuns runs;
  for (std::uint32_t index = 0u; index < WrittenRuns::max_runs; index++) CHECK(runs.note(index * 10u * gap, 1u));
  CHECK_EQ(runs.count(), WrittenRuns::max_runs);
  CHECK(!runs.note(WrittenRuns::max_runs * 10u * gap, 1u));
  CHECK(runs.note(WrittenRuns::max_runs * 10u * gap - 10u * gap + 2u, 1u));
  CHECK_EQ(runs.count(), WrittenRuns::max_runs);
  CHECK(runs.invariants_hold()); }

TEST(written_runs, invariants_catch_each_kind_of_corruption)
{ WrittenRuns runs;
  CHECK(runs.note(0u, 10u));
  CHECK(runs.note(10u * gap, 10u));
  CHECK(runs.invariants_hold());
  WrittenRunsProbe::set_count(runs, WrittenRuns::max_runs + 1u);
  CHECK(!runs.invariants_hold());
  WrittenRunsProbe::set_count(runs, 2u);
  WrittenRunsProbe::set(runs, 0u, 0u, 0u);
  CHECK(!runs.invariants_hold());
  WrittenRunsProbe::set(runs, 0u, 5u, ~std::uint64_t{0});
  CHECK(!runs.invariants_hold());
  WrittenRunsProbe::set(runs, 0u, 0u, 10u);
  WrittenRunsProbe::set(runs, 1u, 5u, 10u);
  CHECK(!runs.invariants_hold());
  WrittenRunsProbe::set(runs, 1u, 10u + gap - 1u, 10u);
  CHECK(!runs.invariants_hold());
  WrittenRunsProbe::set(runs, 1u, 10u + gap, 10u);
  CHECK(runs.invariants_hold()); }
