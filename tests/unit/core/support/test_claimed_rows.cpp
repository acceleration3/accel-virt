#include "core/support/claimed_rows.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <pthread.h>

namespace av {

struct ClaimedRowsProbe {
    template <typename Row>
    static std::uint32_t chunks(const ClaimedRows<Row> &rows)
    {
        std::uint32_t made = 0u;
        for (auto *chunk : rows.chunks_) made += chunk != nullptr;
        return made;
    }

    template <typename Row>
    static void corrupt_claimed(ClaimedRows<Row> &rows, std::uint32_t claimed) { rows.claimed_ = claimed; }

    template <typename Row>
    static void mark_ready(ClaimedRows<Row> &rows, std::uint32_t chunk, std::uint32_t index)
    {
        rows.chunks_[chunk][index].ready_ = true;
    }

    /* A second writer publishing chunk `chunk` first, with memory of its own. */
    template <typename Row>
    static void publish_first(ClaimedRows<Row> &rows, BlockSource &blocks, std::uint32_t chunk)
    {
        using Cell = typename ClaimedRows<Row>::Cell;
        const std::size_t cells = std::size_t{ClaimedRows<Row>::first_rows} << chunk;
        auto *const       made = static_cast<Cell *>(blocks.allocate(sizeof(Cell) * cells, Pool::paged));
        for (std::size_t index = 0u; index < cells; index++) made[index].ready_ = false;
        rows.chunks_[chunk] = made;
    }
};

}  // namespace av

using av::BlockSource;
using av::ClaimedRows;
using av::ClaimedRowsProbe;
using av::Irql;
using av::MockPlatform;
using av::Pool;
using av::Status;
using av::Violation;

namespace {

struct Line {
    const char   *key;
    std::int64_t  code;
};

using Lines = ClaimedRows<Line>;

/* A block source whose first allocation of a chunk loses the race: another
 * writer publishes the chunk while this one is still making its own. */
class RacingBlocks final : public BlockSource {
public:
    explicit RacingBlocks(MockPlatform &platform) : platform_(platform), rows_(nullptr), chunk_(0u) {}

    void race(Lines &rows, std::uint32_t chunk)
    {
        rows_ = &rows;
        chunk_ = chunk;
    }
    void *allocate(std::size_t bytes, Pool pool) override
    {
        Lines *const racing = rows_;
        rows_ = nullptr;
        if (racing != nullptr) ClaimedRowsProbe::publish_first(*racing, platform_, chunk_);
        return platform_.allocate(bytes, pool);
    }
    void release(void *block) override { platform_.release(block); }
    void require_irql_at_most(Irql ceiling, const char *function) override
    {
        platform_.require_irql_at_most(ceiling, function);
    }

private:
    MockPlatform &platform_;
    Lines        *rows_;
    std::uint32_t chunk_;
};

bool holds(const Lines &rows, std::int64_t code)
{
    return rows.find_if([code](const Line &line) { return line.code == code; }) != nullptr;
}

constexpr std::uint32_t writers = 4u;
constexpr std::uint32_t per_writer = 5000u;

struct Writer {
    Lines        *rows;
    std::uint32_t first;
    std::uint32_t failed;
};

void *write_rows(void *argument)
{
    auto *const writer = static_cast<Writer *>(argument);
    for (std::uint32_t code = writer->first; code < writer->first + per_writer; code++)
        if (writer->rows->append(Line{"key", code}) != Status::ok) writer->failed++;
    return nullptr;
}

/* A malloc-backed source for threads: the mock platform keeps its books on one. */
class ThreadBlocks final : public BlockSource {
public:
    void *allocate(std::size_t bytes, Pool) override { return std::calloc(1u, bytes); }
    void  release(void *block) override { std::free(block); }
    void  require_irql_at_most(Irql, const char *) override {}
};

}  // namespace

TEST(claimed_rows_append, starts_with_nothing_and_makes_the_first_chunk_on_the_first_row)
{ MockPlatform platform;
  Lines rows(platform, Pool::paged);
  CHECK(rows.invariants_hold());
  CHECK_EQ(rows.claimed(), 0u);
  CHECK(rows.find_if([](const Line &) { return true; }) == nullptr);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_STATUS(rows.append(Line{"a", 1}), Status::ok);
  CHECK(rows.invariants_hold());
  CHECK_EQ(rows.claimed(), 1u);
  CHECK_EQ(ClaimedRowsProbe::chunks(rows), 1u);
  const Line *const found = rows.find_if([](const Line &line) { return line.code == 1; });
  CHECK(found != nullptr);
  CHECK(found->key[0] == 'a'); }

TEST(claimed_rows_append, grows_well_past_the_old_tables_without_moving_a_row)
{ MockPlatform platform;
  Lines rows(platform, Pool::paged);
  CHECK_STATUS(rows.append(Line{"first", 0}), Status::ok);
  const Line *const first = rows.find_if([](const Line &line) { return line.code == 0; });
  for (std::int64_t code = 1; code < 20000; code++) CHECK_STATUS(rows.append(Line{"k", code}), Status::ok);
  CHECK(rows.invariants_hold());
  CHECK_EQ(rows.claimed(), 20000u);
  CHECK_EQ(ClaimedRowsProbe::chunks(rows), 11u);
  CHECK(rows.find_if([](const Line &line) { return line.code == 0; }) == first);
  CHECK(holds(rows, 255));
  CHECK(holds(rows, 256));
  CHECK(holds(rows, 19999));
  CHECK(!holds(rows, 20000));
  std::int64_t next = 0;
  bool         ordered = true;
  rows.for_each([&](const Line &line) { ordered = ordered && line.code == next++; });
  CHECK(ordered);
  CHECK_EQ(next, 20000); }

TEST(claimed_rows_append, a_chunk_that_cannot_be_made_leaves_its_row_unready_and_the_next_tries_again)
{ MockPlatform platform;
  Lines rows(platform, Pool::paged);
  for (std::int64_t code = 0; code < 16; code++) CHECK_STATUS(rows.append(Line{"k", code}), Status::ok);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(rows.append(Line{"lost", 16}), Status::no_memory);
  CHECK(rows.invariants_hold());
  CHECK_EQ(rows.claimed(), 17u);
  CHECK(!holds(rows, 16));
  CHECK_STATUS(rows.append(Line{"kept", 17}), Status::ok);
  CHECK(holds(rows, 17));
  CHECK(!holds(rows, 16));
  std::uint32_t visited = 0u;
  rows.for_each([&](const Line &) { visited++; });
  CHECK_EQ(visited, 17u);
  CHECK(rows.invariants_hold()); }

TEST(claimed_rows_append, a_writer_that_loses_the_race_for_a_chunk_writes_into_the_winners)
{ MockPlatform platform;
  RacingBlocks blocks(platform);
  { Lines rows(blocks, Pool::paged);
    blocks.race(rows, 0u);
    CHECK_STATUS(rows.append(Line{"k", 7}), Status::ok);
    CHECK_EQ(ClaimedRowsProbe::chunks(rows), 1u);
    CHECK_EQ(platform.live_allocations(), 1u);
    CHECK(holds(rows, 7));
    CHECK(rows.invariants_hold()); }
  CHECK_EQ(platform.live_allocations(), 0u); }

TEST(claimed_rows_append, refuses_past_most_rows_and_stays_exactly_there)
{ MockPlatform platform;
  Lines rows(platform, Pool::paged);
  ClaimedRowsProbe::corrupt_claimed(rows, Lines::most_rows);
  CHECK_STATUS(rows.append(Line{"k", 1}), Status::no_memory);
  CHECK_STATUS(rows.append(Line{"k", 2}), Status::no_memory);
  CHECK_EQ(rows.claimed(), Lines::most_rows);
  CHECK_EQ(platform.live_allocations(), 0u);
  ClaimedRowsProbe::corrupt_claimed(rows, 0u); }

TEST(claimed_rows_append, is_legal_up_to_apc)
{ MockPlatform platform;
  Lines rows(platform, Pool::paged);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(rows.append(Line{"k", 1}));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::allocation_at_dispatch)) {} }

TEST(claimed_rows_append, rows_appended_from_four_threads_while_another_reads_are_all_there_once)
{ ThreadBlocks blocks;
  Lines        rows(blocks, Pool::paged);
  Writer       writing[writers] = {};
  pthread_t    threads[writers] = {};
  for (std::uint32_t index = 0u; index < writers; index++) {
      writing[index] = Writer{&rows, index * per_writer, 0u};
      CHECK_EQ(pthread_create(&threads[index], nullptr, write_rows, &writing[index]), 0);
  }
  std::uint32_t seen = 0u;
  while (seen < writers * per_writer) {
      std::uint32_t ready = 0u;
      rows.for_each([&](const Line &line) { ready += line.key != nullptr; });
      seen = ready;
  }
  for (pthread_t thread : threads) CHECK_EQ(pthread_join(thread, nullptr), 0);
  for (const Writer &writer : writing) CHECK_EQ(writer.failed, 0u);
  CHECK_EQ(rows.claimed(), writers * per_writer);
  static bool found[writers * per_writer];
  std::uint32_t duplicates = 0u;
  rows.for_each([&](const Line &line) {
      duplicates += found[line.code];
      found[line.code] = true;
  });
  CHECK_EQ(duplicates, 0u);
  std::uint32_t missing = 0u;
  for (bool was : found) missing += !was;
  CHECK_EQ(missing, 0u);
  CHECK(rows.invariants_hold()); }

TEST(claimed_rows_limits, bound_chunks_by_what_a_size_t_counts)
{ CHECK_EQ(Lines::most_chunks, 27u);
  CHECK_EQ(Lines::most_rows, 16u * ((1u << 27u) - 1u)); }

TEST(claimed_rows_invariants, fail_for_a_claim_past_most_rows_or_a_ready_row_past_the_claim)
{ MockPlatform platform;
  Lines rows(platform, Pool::paged);
  CHECK_STATUS(rows.append(Line{"k", 1}), Status::ok);
  CHECK_STATUS(rows.append(Line{"k", 2}), Status::ok);
  CHECK(rows.invariants_hold());
  ClaimedRowsProbe::mark_ready(rows, 0u, 5u);
  CHECK(!rows.invariants_hold());
  ClaimedRowsProbe::corrupt_claimed(rows, 6u);
  CHECK(rows.invariants_hold());
  ClaimedRowsProbe::corrupt_claimed(rows, Lines::most_rows + 1u);
  CHECK(!rows.invariants_hold());
  ClaimedRowsProbe::corrupt_claimed(rows, 6u); }
