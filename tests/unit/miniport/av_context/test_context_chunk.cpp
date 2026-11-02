#include "miniport/av_context/context_chunk.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

using av::ContextChunk;
using av::HostBlobId;

namespace {
int         mapped = 0;
void *const mapping = &mapped;

}

TEST(context_chunk, keeps_what_taking_it_down_needs)
{ const ContextChunk chunk(5u, HostBlobId::first + 9u, 12u, 3u, mapping);
  CHECK_EQ(chunk.slot(), 5u);
  CHECK_EQ(chunk.resource_id(), HostBlobId::first + 9u);
  CHECK_EQ(chunk.first_page(), 12u);
  CHECK_EQ(chunk.pages(), 3u);
  CHECK(chunk.mapping() == mapping);
  CHECK(chunk.invariants_hold()); }

TEST(context_chunk, a_chunk_at_page_zero_of_slot_zero_is_whole)
{ const ContextChunk chunk(0u, HostBlobId::first, 0u, 1u, mapping);
  CHECK_EQ(chunk.slot(), 0u);
  CHECK_EQ(chunk.first_page(), 0u);
  CHECK(chunk.invariants_hold()); }

TEST(context_chunk, a_chunk_of_lent_pages_keeps_its_lock_and_no_window)
{ int         lock = 0;
  const ContextChunk chunk(2u, HostBlobId::first + 4u, &lock);
  CHECK_EQ(chunk.slot(), 2u);
  CHECK_EQ(chunk.resource_id(), HostBlobId::first + 4u);
  CHECK(chunk.locked() == &lock);
  CHECK_EQ(chunk.pages(), 0u);
  CHECK_EQ(chunk.first_page(), 0u);
  CHECK(chunk.mapping() == nullptr);
  CHECK(chunk.invariants_hold());
  CHECK(!ContextChunk(2u, 0u, &lock).invariants_hold());
  CHECK(ContextChunk(1u, HostBlobId::first, 4u, 2u, mapping).locked() == nullptr); }

TEST(context_chunk_invariants, a_chunk_with_no_resource_no_pages_or_no_view_is_caught)
{ CHECK(!ContextChunk(1u, 0u, 4u, 2u, mapping).invariants_hold());
  CHECK(!ContextChunk(1u, HostBlobId::first, 4u, 0u, mapping).invariants_hold());
  CHECK(!ContextChunk(1u, HostBlobId::first, 4u, 2u, nullptr).invariants_hold());
  CHECK(ContextChunk(1u, HostBlobId::first, 4u, 2u, mapping).invariants_hold()); }
