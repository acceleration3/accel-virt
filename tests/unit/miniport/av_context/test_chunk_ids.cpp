/* The adapter's host blob ids: every chunk on the device takes one no other
 * resource and no other chunk has, the lowest free first, and gives it back
 * when the device let go of its blob (docs/host-memory.md). */
#include "miniport/av_context/chunk_ids.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

using av::ChunkIds;
using av::HostBlobId;
using av::Status;

TEST(chunk_ids_take, hands_out_the_lowest_free_id_first)
{ ChunkIds ids;
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
  CHECK_EQ(ids.taken(), 2u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 1u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 3u); }

/* The bitmap is words of 64: the 65th id is the first of the second word,
 * and a free bit in a later word is found past a full one. */
TEST(chunk_ids_take, crosses_from_a_full_word_to_the_next)
{ ChunkIds      ids;
  std::uint32_t id = 0u;
  for (std::uint32_t taken = 0u; taken < 64u; taken++) CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 63u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 64u);
  ids.give_back(HostBlobId::first + 64u);
  ids.give_back(HostBlobId::first + 70u);   /* never taken: nothing changes */
  CHECK_EQ(ids.taken(), 64u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 64u); }

/* Every id the device keeps, and then none: the next open has nowhere to go
 * and nothing moved. The last id is the range's own last. */
TEST(chunk_ids_take, every_id_taken_is_no_space)
{ ChunkIds      ids;
  std::uint32_t id = 0u;
  for (std::uint32_t taken = 0u; taken < HostBlobId::count; taken++) CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + HostBlobId::count - 1u);
  CHECK_EQ(ids.taken(), HostBlobId::count);
  std::uint32_t refused = 7u;
  CHECK_STATUS(ids.take(refused), Status::no_space);
  CHECK_EQ(refused, 7u);
  ids.give_back(HostBlobId::first + HostBlobId::count - 1u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + HostBlobId::count - 1u); }

/* An id outside the range names no bit: given back, it frees nothing - not
 * the id a wrap of its slot would name. */
TEST(chunk_ids_give_back, an_id_outside_the_range_frees_nothing)
{ ChunkIds      ids;
  std::uint32_t id = 0u;
  CHECK_STATUS(ids.take(id), Status::ok);
  const std::uint32_t outside[] = {0u, HostBlobId::first - 1u, HostBlobId::first + HostBlobId::count,
                                   HostBlobId::first + 2u * HostBlobId::count, 0xFFFFFFFFu};
  for (const std::uint32_t stray : outside) ids.give_back(stray);
  CHECK_EQ(ids.taken(), 1u);
  CHECK_STATUS(ids.take(id), Status::ok);
  CHECK_EQ(id, HostBlobId::first + 1u); }
