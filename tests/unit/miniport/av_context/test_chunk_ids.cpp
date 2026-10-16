/* The adapter's host blob ids: every chunk on the device takes one no other
 * resource and no other chunk has, the lowest free first, and gives it back
 * when the device let go of its blob (docs/host-memory.md). The bitmap grows
 * as ids are taken, so only the id range itself runs out. */
#include "miniport/av_context/chunk_ids.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

namespace av {

struct ChunkIdsProbe {
    static void set_taken(ChunkIds &ids, std::uint32_t taken) { ids.taken_ = taken; }
    static void set_lowest(ChunkIds &ids, std::uint32_t lowest) { ids.lowest_ = lowest; }
    static void set_word(ChunkIds &ids, std::uint32_t word, std::uint64_t bits) { ids.words_.at(word) = bits; }
};

}  // namespace av

using av::ChunkIds;
using av::ChunkIdsProbe;
using av::HostBlobId;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

TEST(chunk_ids_take, hands_out_the_lowest_free_id_first)
{ MockPlatform platform;
  ChunkIds ids(platform, HostBlobId::most);
  CHECK_EQ(ids.taken(), 0u);
  std::uint32_t id = 0u;
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 1u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 2u);
  CHECK_EQ(ids.taken(), 3u);
  ids.give_back(HostBlobId::first + 1u);
  CHECK(ids.invariants_hold());
  CHECK_EQ(ids.taken(), 2u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 1u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 3u);
  CHECK(ids.invariants_hold()); }

/* The bitmap is words of 64: the 65th id is the first of the second word,
 * and a free bit in a later word is found past a full one. */
TEST(chunk_ids_take, crosses_from_a_full_word_to_the_next)
{ MockPlatform  platform;
  ChunkIds      ids(platform, HostBlobId::most);
  std::uint32_t id = 0u;
  for (std::uint32_t taken = 0u; taken < 64u; taken++) CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 63u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 64u);
  ids.give_back(HostBlobId::first + 64u);
  ids.give_back(HostBlobId::first + 70u);   /* never taken: nothing changes */
  CHECK_EQ(ids.taken(), 64u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 64u);
  CHECK(ids.invariants_hold()); }

/* The 16384 ids the device once kept are no bound: ids are handed out well
 * past them, each once, and one given back low down is the next taken. */
TEST(chunk_ids_take, hands_out_ids_well_past_the_old_range)
{ MockPlatform  platform;
  ChunkIds      ids(platform, HostBlobId::most);
  std::uint32_t id = 0u;
  for (std::uint32_t taken = 0u; taken < 100000u; taken++) {
      CHECK_STATUS(ids.take(id), Status::ok);
      if (id != HostBlobId::first + taken) CHECK_EQ(id, HostBlobId::first + taken);
  }
  CHECK_EQ(ids.taken(), 100000u);
  CHECK(ids.invariants_hold());
  ids.give_back(HostBlobId::first + 20000u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 20000u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 100000u);
  CHECK(ids.invariants_hold()); }

/* Every id of the range taken, and then none: the next open has nowhere to
 * go and nothing moved. The range here is short, as a test can take it. */
TEST(chunk_ids_take, every_id_of_the_range_taken_is_no_space)
{ MockPlatform  platform;
  ChunkIds      ids(platform, 70u);
  std::uint32_t id = 0u;
  for (std::uint32_t taken = 0u; taken < 70u; taken++) CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 69u);
  std::uint32_t refused = 7u;
  CHECK_STATUS(ids.take(refused), Status::no_space);
  CHECK_STATUS(ids.take(refused), Status::no_space);
  CHECK_EQ(refused, 7u);
  CHECK_EQ(ids.taken(), 70u);
  CHECK(ids.invariants_hold());
  ids.give_back(HostBlobId::first + 69u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 69u); }

/* A bitmap that cannot grow to note the next id refuses it as out of memory,
 * hands out nothing and takes the next once memory is back. */
TEST(chunk_ids_take, a_bitmap_that_cannot_grow_is_out_of_memory)
{ MockPlatform  platform;
  ChunkIds      ids(platform, HostBlobId::most);
  std::uint32_t id = 0u;
  platform.fail_allocation_at(1u);
  CHECK_STATUS(ids.take(id), Status::no_memory);
  CHECK_EQ(id, 0u);
  CHECK_EQ(ids.taken(), 0u);
  CHECK(ids.invariants_hold());
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first); }

TEST(chunk_ids_take, is_passive_only)
{ MockPlatform  platform;
  ChunkIds      ids(platform, HostBlobId::most);
  std::uint32_t id = 0u;
  platform.set_irql(Irql::apc);
  static_cast<void>(ids.take(id));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

/* An id outside the range names no bit, and neither does one past the words
 * the bitmap has: given back, each frees nothing. */
TEST(chunk_ids_give_back, an_id_outside_the_range_or_past_the_bitmap_frees_nothing)
{ MockPlatform  platform;
  ChunkIds      ids(platform, HostBlobId::most);
  std::uint32_t id = 0u;
  CHECK_STATUS(ids.take(id), Status::ok);
  const std::uint32_t outside[] = {0u, HostBlobId::first - 1u, HostBlobId::first + 64u, 0xFFFFFFFFu};
  for (const std::uint32_t stray : outside) ids.give_back(stray);
  CHECK_EQ(ids.taken(), 1u);
  CHECK(ids.invariants_hold());
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 1u); }

TEST(chunk_ids_invariants, fail_for_a_wrong_count_a_free_bit_below_the_search_or_a_bit_past_the_range)
{ MockPlatform  platform;
  ChunkIds      ids(platform, 100u);
  std::uint32_t id = 0u;
  for (std::uint32_t taken = 0u; taken < 66u; taken++) CHECK_STATUS(ids.take(id), Status::ok);
  CHECK(ids.invariants_hold());
  ChunkIdsProbe::set_taken(ids, 65u);
  CHECK(!ids.invariants_hold());
  ChunkIdsProbe::set_taken(ids, 66u);
  ChunkIdsProbe::set_word(ids, 0u, ~std::uint64_t{0} - 1u);
  CHECK(!ids.invariants_hold());
  ChunkIdsProbe::set_lowest(ids, 0u);
  ChunkIdsProbe::set_taken(ids, 65u);
  CHECK(ids.invariants_hold());
  ChunkIdsProbe::set_word(ids, 1u, std::uint64_t{1} << 40u | 3u);
  ChunkIdsProbe::set_taken(ids, 66u);
  CHECK(!ids.invariants_hold()); }
