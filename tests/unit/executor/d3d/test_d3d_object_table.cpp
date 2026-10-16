/* Every Direct3D object one session made, by the id the guest named it with:
 * an id answers only for the kind it was made as, each entry holds one
 * reference, which a removal and a clear give back, and the views are listed
 * apart for a swap chain's rotation. Objects here are counted stand-ins, so a
 * reference given back is a count that moved. */
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

#include <sys/resource.h>

using av::D3dObjectKind;
using av::D3dObjectTable;

namespace av {

struct D3dObjectTableProbe {
    static bool mapped(const D3dObjectTable &table) { return table.table_ != nullptr; }
    static bool tombstone(const D3dObjectTable &table, std::uint32_t slot) { return table.tombstones_[slot]; }
    static void counts(D3dObjectTable &table, std::uint32_t count, std::uint32_t views)
    {
        table.count_ = count;
        table.views_ = views;
    }
};

}  // namespace av

using av::D3dObjectTableProbe;

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

/* One table, cleared at the start and end of each test, before the test's
 * objects go: a full one is eight megabytes of pages. */
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

TEST(d3d_object_table_find_view, answers_each_view_kind_and_nothing_else)
{ D3dObjectTable &table = fresh_table();
  Counted         views[4], buffer;
  const D3dObjectKind kinds[4] = {D3dObjectKind::shader_resource_view, D3dObjectKind::render_target_view,
                                  D3dObjectKind::depth_stencil_view, D3dObjectKind::unordered_access_view};
  for (std::uint32_t index = 0u; index < 4u; index++) CHECK(table.insert(index + 1u, kinds[index], &views[index]));
  CHECK(table.insert(9u, D3dObjectKind::resource, &buffer));
  for (std::uint32_t index = 0u; index < 4u; index++) CHECK(table.find_view(index + 1u) == &views[index]);
  CHECK(table.find_view(9u) == nullptr);
  CHECK(table.find_view(10u) == nullptr);
  CHECK(table.find_view(0u) == nullptr);
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

/* A table nothing was put in has no memory and answers as an empty one. */
TEST(d3d_object_table_mapping, an_unused_table_holds_no_memory_and_answers_empty)
{ D3dObjectTable table;
  CHECK(!D3dObjectTableProbe::mapped(table));
  CHECK(table.find(1u, D3dObjectKind::resource) == nullptr);
  D3dObjectKind kind = D3dObjectKind::resource;
  CHECK(table.find_any(1u, kind) == nullptr);
  CHECK(kind == D3dObjectKind::none);
  CHECK(table.find_view(1u) == nullptr);
  Counted object;
  CHECK(table.exchange(1u, D3dObjectKind::resource, &object) == nullptr);
  CHECK_EQ(table.next_used(0u), D3dObjectTable::capacity);
  CHECK_EQ(table.view_slot(0u), D3dObjectTable::capacity);
  table.note_viewed(0u, &object);
  CHECK(table.viewed(0u) == nullptr);
  std::uint32_t id = 9u;
  CHECK(table.at(0u, id, kind) == nullptr);
  CHECK_EQ(id, 0u);
  table.remove(1u);
  table.remove_kinds(D3dObjectKind::resource, D3dObjectKind::query9);
  CHECK_EQ(table.count_kinds(D3dObjectKind::resource, D3dObjectKind::query9), 0u);
  table.clear();
  CHECK(!D3dObjectTableProbe::mapped(table));
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  /* The first object maps it. */
  CHECK(table.insert(1u, D3dObjectKind::resource, &object));
  CHECK(D3dObjectTableProbe::mapped(table));
  CHECK(table.find(1u, D3dObjectKind::resource) == &object);
  table.clear(); }

/* The address space closed to new mappings for the one call: the object is
 * refused - kept, the made one released - and taken once memory can be had. */
TEST(d3d_object_table_mapping, a_table_no_memory_can_be_had_for_refuses_the_object)
{ D3dObjectTable table;
  Counted made;
  rlimit was = {};
  CHECK_EQ(getrlimit(RLIMIT_AS, &was), 0);
  rlimit closed = was;
  closed.rlim_cur = 0u;
  CHECK_EQ(setrlimit(RLIMIT_AS, &closed), 0);
  const bool kept = table.keep(4u, D3dObjectKind::resource, &made);
  CHECK_EQ(setrlimit(RLIMIT_AS, &was), 0);
  CHECK(!kept);
  CHECK_EQ(made.released, 1u);
  CHECK(!D3dObjectTableProbe::mapped(table));
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  Counted again;
  CHECK(table.keep(4u, D3dObjectKind::resource, &again));
  CHECK_EQ(table.count(), 1u);
  table.clear(); }

/* A clear gives back every reference and every page: the holes removals
 * left are gone too, the views forgotten, and the table maps the same memory
 * on. */
TEST(d3d_object_table_mapping, a_clear_leaves_no_object_view_or_hole_behind)
{ D3dObjectTable table;
  Counted gone, view;
  CHECK(table.insert(1u, D3dObjectKind::resource, &gone));
  CHECK(table.insert(2u, D3dObjectKind::shader_resource_view, &view));
  table.note_viewed(table.view_slot(0u), &gone);
  std::uint32_t id = 0u;
  D3dObjectKind kind = D3dObjectKind::none;
  std::uint32_t hole = D3dObjectTable::capacity;
  for (std::uint32_t slot = table.next_used(0u); slot < D3dObjectTable::capacity; slot = table.next_used(slot + 1u))
      if (table.at(slot, id, kind) == &gone) hole = slot;
  table.remove(1u);
  CHECK(D3dObjectTableProbe::tombstone(table, hole));
  const std::uint32_t view_slot = table.view_slot(0u);
  table.clear();
  CHECK_EQ(view.released, 1u);
  CHECK(D3dObjectTableProbe::mapped(table));
  CHECK(!D3dObjectTableProbe::tombstone(table, hole));
  CHECK(table.viewed(view_slot) == nullptr);
  CHECK_EQ(table.views(), 0u);
  CHECK_EQ(table.next_used(0u), D3dObjectTable::capacity);
  CHECK(table.invariants_hold()); }

/* An unmapped table holds nothing: a count or a view says otherwise. */
TEST(d3d_object_table_mapping, the_invariant_catches_an_unmapped_table_that_claims_objects)
{ D3dObjectTable counted;
  D3dObjectTableProbe::counts(counted, 1u, 0u);
  CHECK(!counted.invariants_hold());
  D3dObjectTable viewing;
  D3dObjectTableProbe::counts(viewing, 0u, 1u);
  CHECK(!viewing.invariants_hold()); }
