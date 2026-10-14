/* One chunk of the host window an av context's process holds
 * (docs/host-memory.md): the host blob the device made for the context, the
 * run of window pages it is placed at, and the process's view of them - all
 * that taking it down needs, kept exactly as it was made. */
#include "miniport/av_context/context_chunk.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/resources/host_blob_id.hpp"

#include <cstdint>

using av::ContextChunk;
using av::HostBlobId;

namespace {

/* Never dereferenced: a chunk only names the process's view. */
int         mapped = 0;
void *const mapping = &mapped;

}  // namespace

TEST(context_chunk, keeps_what_taking_it_down_needs)
{ const ContextChunk chunk(5u, HostBlobId::first + 9u, 12u, 3u, mapping);
  CHECK_EQ(chunk.slot(), 5u);
  CHECK_EQ(chunk.resource_id(), HostBlobId::first + 9u);
  CHECK_EQ(chunk.first_page(), 12u);
  CHECK_EQ(chunk.pages(), 3u);
  CHECK(chunk.mapping() == mapping);
  CHECK(chunk.invariants_hold()); }

/* The run may start at the window's first page and the context may be slot
 * 0: neither is a missing field. */
TEST(context_chunk, a_chunk_at_page_zero_of_slot_zero_is_whole)
{ const ContextChunk chunk(0u, HostBlobId::first, 0u, 1u, mapping);
  CHECK_EQ(chunk.slot(), 0u);
  CHECK_EQ(chunk.first_page(), 0u);
  CHECK(chunk.invariants_hold()); }

/* Each of the three things a chunk cannot be without, missing on its own,
 * is caught on its own. */
TEST(context_chunk_invariants, a_chunk_with_no_resource_no_pages_or_no_view_is_caught)
{ CHECK(!ContextChunk(1u, 0u, 4u, 2u, mapping).invariants_hold());
  CHECK(!ContextChunk(1u, HostBlobId::first, 4u, 0u, mapping).invariants_hold());
  CHECK(!ContextChunk(1u, HostBlobId::first, 4u, 2u, nullptr).invariants_hold());
  CHECK(ContextChunk(1u, HostBlobId::first, 4u, 2u, mapping).invariants_hold()); }
