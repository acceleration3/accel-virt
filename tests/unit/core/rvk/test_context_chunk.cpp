/* One chunk slot of an rvk context: free until it takes a RAM chunk (guest RAM
 * the driver owns, feature 045) or a window chunk (pages of the host window and
 * their process mapping, docs/host-window.md), free again when cleared. */
#include "core/rvk/context_chunk.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace av {

struct ContextChunkProbe {
    static void set_resource(ContextChunk &chunk, std::uint32_t resource) { chunk.resource_id_ = resource; }
    static void set_pages(ContextChunk &chunk, std::uint32_t first, std::uint32_t pages)
    {
        chunk.first_page_ = first;
        chunk.pages_ = pages;
    }
    static void set_mapping(ContextChunk &chunk, void *mapping) { chunk.mapping_ = mapping; }
};

}  // namespace av

using av::ContextChunk;
using av::ContextChunkProbe;

namespace {

/* Never dereferenced: a slot only names the pages and the mapping. */
int                    storage = 0;
int                    mapped = 0;
av::GuestPages *const  somewhere = static_cast<av::GuestPages *>(static_cast<void *>(&storage));
void *const                 mapping = &mapped;

}  // namespace

TEST(context_chunk, holds_a_ram_chunk_until_cleared)
{ ContextChunk slot;
  CHECK(!slot.bound());
  CHECK(!slot.in_window());
  CHECK(slot.ram() == nullptr);
  CHECK_EQ(slot.resource_id(), 0u);
  CHECK(slot.invariants_hold());
  slot.take_ram(0x00100003u, *somewhere);
  CHECK(slot.bound());
  CHECK(!slot.in_window());
  CHECK(slot.ram() == somewhere);
  CHECK(slot.mapping() == nullptr);
  CHECK_EQ(slot.resource_id(), 0x00100003u);
  CHECK_EQ(slot.first_page(), 0u);
  CHECK_EQ(slot.pages(), 0u);
  CHECK(slot.invariants_hold());
  slot.clear();
  CHECK(!slot.bound());
  CHECK(slot.ram() == nullptr);
  CHECK_EQ(slot.resource_id(), 0u);
  CHECK(slot.invariants_hold()); }

TEST(context_chunk, holds_a_window_chunk_until_cleared)
{ ContextChunk slot;
  slot.take_window(0x00100004u, 12u, 3u, mapping);
  CHECK(slot.bound());
  CHECK(slot.in_window());
  CHECK(slot.ram() == nullptr);
  CHECK(slot.mapping() == mapping);
  CHECK_EQ(slot.resource_id(), 0x00100004u);
  CHECK_EQ(slot.first_page(), 12u);
  CHECK_EQ(slot.pages(), 3u);
  CHECK(slot.invariants_hold());
  /* A slot is one kind at a time: a RAM chunk taken over it forgets the window. */
  slot.take_ram(0x00100004u, *somewhere);
  CHECK(!slot.in_window());
  CHECK_EQ(slot.pages(), 0u);
  CHECK(slot.invariants_hold());
  slot.take_window(0x00100004u, 12u, 3u, mapping);
  slot.clear();
  CHECK(!slot.bound());
  CHECK(!slot.in_window());
  CHECK_EQ(slot.first_page(), 0u);
  CHECK_EQ(slot.pages(), 0u);
  CHECK(slot.invariants_hold()); }

TEST(context_chunk_invariants, a_slot_that_is_neither_or_both_kinds_breaks_them)
{ /* A resource with nothing behind it. */
  ContextChunk bare;
  ContextChunkProbe::set_resource(bare, 7u);
  CHECK(!bare.invariants_hold());
  /* Pages, or a window, with no resource. */
  ContextChunk guest;
  guest.take_ram(7u, *somewhere);
  ContextChunkProbe::set_resource(guest, 0u);
  CHECK(!guest.invariants_hold());
  ContextChunk window;
  window.take_window(7u, 1u, 1u, mapping);
  ContextChunkProbe::set_resource(window, 0u);
  CHECK(!window.invariants_hold());
  /* Free, but still naming pages. */
  ContextChunk stale;
  ContextChunkProbe::set_pages(stale, 0u, 1u);
  CHECK(!stale.invariants_hold());
  ContextChunkProbe::set_pages(stale, 1u, 0u);
  CHECK(!stale.invariants_hold());
  /* Both kinds at once, or a RAM chunk naming window pages. */
  ContextChunk both;
  both.take_ram(7u, *somewhere);
  ContextChunkProbe::set_mapping(both, mapping);
  CHECK(!both.invariants_hold());
  ContextChunk paged;
  paged.take_ram(7u, *somewhere);
  ContextChunkProbe::set_pages(paged, 0u, 2u);
  CHECK(!paged.invariants_hold());
  ContextChunkProbe::set_pages(paged, 2u, 0u);
  CHECK(!paged.invariants_hold());
  /* A window chunk of no pages. */
  ContextChunk empty;
  empty.take_window(7u, 4u, 0u, mapping);
  CHECK(!empty.invariants_hold()); }
