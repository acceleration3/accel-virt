/* Every Direct3D object one session made, by the id the guest named it with:
 * an id answers only for the kind it was made as, each entry holds one
 * reference, which a removal and a clear give back, and the views are listed
 * apart for a swap chain's rotation. Objects here are counted stand-ins, so a
 * reference given back is a count that moved. */
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::D3dObjectKind;
using av::D3dObjectTable;

namespace {

class Counted final : public IUnknown {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **object) override
    {
        *object = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        ++released;
        return --references;
    }

    ULONG references = 1u;
    ULONG released = 0u;
};

/* The table is most of a megabyte: one, cleared at the start and end of each
 * test, before the test's objects go. */
D3dObjectTable &fresh_table()
{
    static D3dObjectTable table;
    table.clear();
    return table;
}

}  // namespace

TEST(d3d_object_table_insert, an_id_answers_for_the_kind_it_was_made_as_and_nothing_else)
{ D3dObjectTable &table = fresh_table();
  Counted buffer, view;
  CHECK(table.insert(7u, D3dObjectKind::resource, &buffer));
  CHECK(table.insert(8u, D3dObjectKind::shader_resource_view, &view));
  CHECK(table.find(7u, D3dObjectKind::resource) == &buffer);
  CHECK(table.find(7u, D3dObjectKind::shader_resource_view) == nullptr);
  CHECK(table.find(9u, D3dObjectKind::resource) == nullptr);
  CHECK(table.find(0u, D3dObjectKind::none) == nullptr);
  CHECK_EQ(table.count(), 2u);
  CHECK_EQ(buffer.references, 1u);   /* the caller's reference taken, none added */
  CHECK(table.invariants_hold());
  table.clear(); }

TEST(d3d_object_table_insert, refuses_id_zero_a_kind_of_none_no_object_and_an_id_in_use)
{ D3dObjectTable &table = fresh_table();
  Counted one, two;
  CHECK(!table.insert(0u, D3dObjectKind::resource, &one));
  CHECK(!table.insert(1u, D3dObjectKind::none, &one));
  CHECK(!table.insert(1u, D3dObjectKind::resource, nullptr));
  CHECK(table.insert(1u, D3dObjectKind::resource, &one));
  CHECK(!table.insert(1u, D3dObjectKind::sampler, &two));
  CHECK(table.find(1u, D3dObjectKind::resource) == &one);
  CHECK_EQ(table.count(), 1u);
  CHECK_EQ(two.references, 1u);
  table.clear(); }

TEST(d3d_object_table_find_any, answers_the_object_and_its_kind_or_none)
{ D3dObjectTable &table = fresh_table();
  Counted state;
  CHECK(table.insert(40u, D3dObjectKind::blend_state, &state));
  D3dObjectKind kind = D3dObjectKind::query;
  CHECK(table.find_any(40u, kind) == &state);
  CHECK(kind == D3dObjectKind::blend_state);
  CHECK(table.find_any(41u, kind) == nullptr);
  CHECK(kind == D3dObjectKind::none);
  kind = D3dObjectKind::query;
  CHECK(table.find_any(0u, kind) == nullptr);
  CHECK(kind == D3dObjectKind::none);
  table.clear(); }

TEST(d3d_object_table_remove, gives_the_reference_back_and_a_probe_carries_on_past_the_hole)
{ D3dObjectTable &table = fresh_table();
  /* A removal leaves a tombstone, so every id placed past the removed one's
   * slot is still found. */
  Counted objects[64];
  for (std::uint32_t id = 1u; id <= 64u; id++) CHECK(table.insert(id, D3dObjectKind::query, &objects[id - 1u]));
  table.remove(1u);
  CHECK_EQ(objects[0].references, 0u);
  CHECK(table.find(1u, D3dObjectKind::query) == nullptr);
  for (std::uint32_t id = 2u; id <= 64u; id++) CHECK(table.find(id, D3dObjectKind::query) == &objects[id - 1u]);
  CHECK_EQ(table.count(), 63u);
  table.remove(1u);   /* unknown now: nothing */
  table.remove(0u);
  CHECK_EQ(objects[0].references, 0u);
  objects[0].AddRef();   /* a new reference, given to the table */
  CHECK(table.insert(1u, D3dObjectKind::query, &objects[0]));
  CHECK(table.invariants_hold());
  table.clear();
  for (const Counted &object : objects) CHECK_EQ(object.references, 0u);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  table.clear(); }

TEST(d3d_object_table_exchange, swaps_an_object_of_the_same_kind_and_hands_back_the_old)
{ D3dObjectTable &table = fresh_table();
  Counted first, second;
  CHECK(table.insert(5u, D3dObjectKind::render_target_view, &first));
  const std::uint32_t slot = table.view_slot(0u);
  table.note_viewed(slot, &first);
  CHECK(table.exchange(5u, D3dObjectKind::shader_resource_view, &second) == nullptr);
  CHECK(table.exchange(5u, D3dObjectKind::render_target_view, nullptr) == nullptr);
  CHECK(table.exchange(0u, D3dObjectKind::render_target_view, &second) == nullptr);
  CHECK(table.exchange(6u, D3dObjectKind::render_target_view, &second) == nullptr);
  CHECK(table.viewed(slot) == &first);
  CHECK(table.exchange(5u, D3dObjectKind::render_target_view, &second) == &first);
  CHECK(table.find(5u, D3dObjectKind::render_target_view) == &second);
  CHECK(table.viewed(slot) == nullptr);   /* what the old view saw is forgotten */
  CHECK_EQ(first.references, 1u);        /* handed back, not released */
  CHECK_EQ(table.count(), 1u);
  table.clear(); }

/* The four view kinds are listed apart, in insertion order until one goes,
 * when the last takes its place; nothing else is. */
TEST(d3d_object_table_views, lists_every_view_slot_and_only_those)
{ D3dObjectTable &table = fresh_table();
  Counted objects[6];
  CHECK(table.insert(1u, D3dObjectKind::resource, &objects[0]));
  CHECK(table.insert(2u, D3dObjectKind::shader_resource_view, &objects[1]));
  CHECK(table.insert(3u, D3dObjectKind::render_target_view, &objects[2]));
  CHECK(table.insert(4u, D3dObjectKind::depth_stencil_view, &objects[3]));
  CHECK(table.insert(5u, D3dObjectKind::unordered_access_view, &objects[4]));
  CHECK(table.insert(6u, D3dObjectKind::sampler, &objects[5]));
  CHECK_EQ(table.views(), 4u);
  std::uint32_t id = 0u;
  D3dObjectKind kind = D3dObjectKind::none;
  for (std::uint32_t index = 0u; index < 4u; index++) {
      CHECK(table.at(table.view_slot(index), id, kind) == &objects[index + 1u]);
      CHECK_EQ(id, index + 2u);
  }
  CHECK_EQ(table.view_slot(4u), D3dObjectTable::capacity);
  table.remove(3u);
  CHECK_EQ(table.views(), 3u);
  CHECK(table.at(table.view_slot(1u), id, kind) == &objects[4]);   /* the last moved into its place */
  CHECK(kind == D3dObjectKind::unordered_access_view);
  table.clear();
  CHECK_EQ(table.views(), 0u);
  table.clear(); }

TEST(d3d_object_table_note_viewed, remembers_what_a_view_slot_sees_until_the_slot_changes)
{ D3dObjectTable &table = fresh_table();
  Counted view, resource;
  CHECK(table.insert(9u, D3dObjectKind::shader_resource_view, &view));
  const std::uint32_t slot = table.view_slot(0u);
  CHECK(table.viewed(slot) == nullptr);
  table.note_viewed(slot, &resource);
  CHECK(table.viewed(slot) == &resource);
  table.note_viewed(D3dObjectTable::capacity, &resource);   /* outside: nothing */
  CHECK(table.viewed(D3dObjectTable::capacity) == nullptr);
  table.remove(9u);
  CHECK(table.viewed(slot) == nullptr);
  table.clear(); }

TEST(d3d_object_table_next_used, walks_the_used_slots_in_order)
{ D3dObjectTable &table = fresh_table();
  CHECK_EQ(table.next_used(0u), D3dObjectTable::capacity);
  Counted objects[3];
  for (std::uint32_t id = 1u; id <= 3u; id++) CHECK(table.insert(id * 100u, D3dObjectKind::fence12, &objects[id - 1u]));
  std::uint32_t seen = 0u;
  std::uint32_t previous = 0u;
  for (std::uint32_t slot = table.next_used(0u); slot < D3dObjectTable::capacity; slot = table.next_used(slot + 1u)) {
      std::uint32_t id = 0u;
      D3dObjectKind kind = D3dObjectKind::none;
      CHECK(table.at(slot, id, kind) != nullptr);
      CHECK(kind == D3dObjectKind::fence12);
      CHECK(seen == 0u || slot > previous);
      previous = slot;
      seen++;
  }
  CHECK_EQ(seen, 3u);
  std::uint32_t id = 7u;
  D3dObjectKind kind = D3dObjectKind::fence12;
  CHECK(table.at(D3dObjectTable::capacity, id, kind) == nullptr);
  CHECK_EQ(id, 0u);
  CHECK(kind == D3dObjectKind::none);
  table.clear(); }

/* Three quarters full is where it stops taking objects. */
TEST(d3d_object_table_insert, a_table_three_quarters_full_refuses_the_next)
{ D3dObjectTable &table = fresh_table();
  static Counted object;
  object.released = 0u;
  const std::uint32_t most = D3dObjectTable::capacity / 4u * 3u;
  for (std::uint32_t id = 1u; id <= most; id++) CHECK(table.insert(id, D3dObjectKind::sampler, &object));
  CHECK(!table.insert(most + 1u, D3dObjectKind::sampler, &object));
  CHECK_EQ(table.count(), most);
  table.clear();
  CHECK_EQ(object.released, most);
  table.clear(); }

/* What a create made is the table's, or let go: a null - a refused create -
 * is no object, and one the table cannot keep loses the reference it came
 * with rather than leaking it. */
TEST(d3d_object_table_keep, keeps_what_was_made_or_releases_it)
{ D3dObjectTable &table = fresh_table();
  Counted made, twin;
  CHECK(!table.keep(5u, D3dObjectKind::resource, nullptr));
  CHECK(table.keep(5u, D3dObjectKind::resource, &made));
  CHECK_EQ(made.released, 0u);
  CHECK(table.find(5u, D3dObjectKind::resource) == &made);
  CHECK(!table.keep(5u, D3dObjectKind::resource, &twin));
  CHECK_EQ(twin.released, 1u);
  CHECK(table.find(5u, D3dObjectKind::resource) == &made);
  CHECK(table.invariants_hold());
  table.clear(); }

TEST(d3d_object_table_find_as, answers_the_object_as_the_interface_its_kind_is_kept_as)
{ D3dObjectTable &table = fresh_table();
  Counted buffer;
  CHECK(table.insert(3u, D3dObjectKind::resource, &buffer));
  CHECK(table.find_as<Counted>(3u, D3dObjectKind::resource) == &buffer);
  CHECK(table.find_as<Counted>(3u, D3dObjectKind::shader_resource_view) == nullptr);
  table.clear(); }

/* One runtime's objects counted and let go together, the rest kept. */
TEST(d3d_object_table_kinds, count_and_remove_a_range_of_kinds_only)
{ D3dObjectTable &table = fresh_table();
  Counted resource, surface, query;
  CHECK(table.insert(1u, D3dObjectKind::resource, &resource));
  CHECK(table.insert(2u, D3dObjectKind::surface9, &surface));
  CHECK(table.insert(3u, D3dObjectKind::query9, &query));
  CHECK_EQ(table.count_kinds(D3dObjectKind::surface9, D3dObjectKind::query9), 2u);
  CHECK_EQ(table.count_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12), 0u);
  /* A range in the middle passes the kinds either side of it. */
  Counted heap;
  CHECK(table.insert(4u, D3dObjectKind::heap12, &heap));
  CHECK_EQ(table.count_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12), 1u);
  table.remove_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12);
  CHECK_EQ(heap.released, 1u);
  CHECK_EQ(table.count(), 3u);
  table.remove_kinds(D3dObjectKind::surface9, D3dObjectKind::query9);
  CHECK_EQ(surface.released, 1u);
  CHECK_EQ(query.released, 1u);
  CHECK_EQ(resource.released, 0u);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(1u, D3dObjectKind::resource) == &resource);
  CHECK(table.invariants_hold());
  table.clear(); }
