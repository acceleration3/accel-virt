/* Every Direct3D object one session made, by the id the guest named it with:
 * an id answers only for the kind it was made as, each entry holds one
 * reference, which a removal and a clear give back, and the views are listed
 * apart for a swap chain's rotation. There is no count past which an object
 * is refused; a table that cannot grow refuses with no_memory and changes
 * nothing. Objects here are counted stand-ins, so a reference given back is a
 * count that moved. */
#include "executor/d3d/d3d_object_table.hpp"
#include "executor/shim/d3d_headers.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::D3dObjectKind;
using av::D3dObjectTable;
using av::MockPlatform;
using av::Status;

namespace av {

struct D3dObjectTableProbe {
    /* A view's place in the list, as an entry remembers it. */
    static void misplace_view(D3dObjectTable &table, std::uint32_t id, std::uint32_t at)
    {
        table.objects_.find(id)->view_at_ = at;
    }
    static void list_slot(D3dObjectTable &table, std::uint32_t slot) { static_cast<void>(table.views_.append(slot)); }
    static void unlist_views(D3dObjectTable &table) { table.views_.truncate(0u); }
    static void drop_object(D3dObjectTable &table, std::uint32_t id) { table.objects_.find(id)->object_ = nullptr; }
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

}  // namespace

TEST(d3d_object_table_insert, an_id_answers_for_the_kind_it_was_made_as_and_nothing_else)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted buffer, view;
  CHECK_STATUS(table.insert(7u, D3dObjectKind::resource, &buffer), Status::ok);
  CHECK_STATUS(table.insert(8u, D3dObjectKind::shader_resource_view, &view), Status::ok);
  CHECK(table.find(7u, D3dObjectKind::resource) == &buffer);
  CHECK(table.find(7u, D3dObjectKind::shader_resource_view) == nullptr);
  CHECK(table.find(9u, D3dObjectKind::resource) == nullptr);
  CHECK(table.find(0u, D3dObjectKind::none) == nullptr);
  CHECK_EQ(table.count(), 2u);
  CHECK_EQ(buffer.references, 1u);   /* the caller's reference taken, none added */
  CHECK(table.invariants_hold());
  table.clear(); }

TEST(d3d_object_table_insert, refuses_id_zero_a_kind_of_none_no_object_and_an_id_in_use)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted one, two;
  CHECK_STATUS(table.insert(0u, D3dObjectKind::resource, &one), Status::invalid_parameter);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::none, &one), Status::invalid_parameter);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::resource, nullptr), Status::invalid_parameter);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::resource, &one), Status::ok);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::sampler, &two), Status::conflict);
  CHECK(table.find(1u, D3dObjectKind::resource) == &one);
  CHECK_EQ(table.count(), 1u);
  CHECK_EQ(two.references, 1u);
  table.clear(); }

TEST(d3d_object_table_find_any, answers_the_object_and_its_kind_or_none)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted state;
  CHECK_STATUS(table.insert(40u, D3dObjectKind::blend_state, &state), Status::ok);
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
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted         views[4], buffer;
  const D3dObjectKind kinds[4] = {D3dObjectKind::shader_resource_view, D3dObjectKind::render_target_view,
                                  D3dObjectKind::depth_stencil_view, D3dObjectKind::unordered_access_view};
  for (std::uint32_t index = 0u; index < 4u; index++) CHECK_STATUS(table.insert(index + 1u, kinds[index], &views[index]), Status::ok);
  CHECK_STATUS(table.insert(9u, D3dObjectKind::resource, &buffer), Status::ok);
  for (std::uint32_t index = 0u; index < 4u; index++) CHECK(table.find_view(index + 1u) == &views[index]);
  CHECK(table.find_view(9u) == nullptr);
  CHECK(table.find_view(10u) == nullptr);
  CHECK(table.find_view(0u) == nullptr);
  table.clear(); }

TEST(d3d_object_table_remove, gives_the_reference_back_and_every_other_id_is_still_found)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted objects[64];
  for (std::uint32_t id = 1u; id <= 64u; id++) CHECK_STATUS(table.insert(id, D3dObjectKind::query, &objects[id - 1u]), Status::ok);
  table.remove(1u);
  CHECK_EQ(objects[0].references, 0u);
  CHECK(table.find(1u, D3dObjectKind::query) == nullptr);
  for (std::uint32_t id = 2u; id <= 64u; id++) CHECK(table.find(id, D3dObjectKind::query) == &objects[id - 1u]);
  CHECK_EQ(table.count(), 63u);
  table.remove(1u);   /* unknown now: nothing */
  table.remove(0u);
  CHECK_EQ(objects[0].references, 0u);
  objects[0].AddRef();   /* a new reference, given to the table */
  CHECK_STATUS(table.insert(1u, D3dObjectKind::query, &objects[0]), Status::ok);
  CHECK(table.invariants_hold());
  table.clear();
  for (const Counted &object : objects) CHECK_EQ(object.references, 0u);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  table.clear(); }

TEST(d3d_object_table_exchange, swaps_an_object_of_the_same_kind_and_hands_back_the_old)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted first, second;
  CHECK_STATUS(table.insert(5u, D3dObjectKind::render_target_view, &first), Status::ok);
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
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted objects[6];
  CHECK_STATUS(table.insert(1u, D3dObjectKind::resource, &objects[0]), Status::ok);
  CHECK_STATUS(table.insert(2u, D3dObjectKind::shader_resource_view, &objects[1]), Status::ok);
  CHECK_STATUS(table.insert(3u, D3dObjectKind::render_target_view, &objects[2]), Status::ok);
  CHECK_STATUS(table.insert(4u, D3dObjectKind::depth_stencil_view, &objects[3]), Status::ok);
  CHECK_STATUS(table.insert(5u, D3dObjectKind::unordered_access_view, &objects[4]), Status::ok);
  CHECK_STATUS(table.insert(6u, D3dObjectKind::sampler, &objects[5]), Status::ok);
  CHECK_EQ(table.views(), 4u);
  std::uint32_t id = 0u;
  D3dObjectKind kind = D3dObjectKind::none;
  for (std::uint32_t index = 0u; index < 4u; index++) {
      CHECK(table.at(table.view_slot(index), id, kind) == &objects[index + 1u]);
      CHECK_EQ(id, index + 2u);
  }
  CHECK_EQ(table.view_slot(4u), D3dObjectTable::no_slot);
  table.remove(3u);
  CHECK_EQ(table.views(), 3u);
  CHECK(table.at(table.view_slot(1u), id, kind) == &objects[4]);   /* the last moved into its place */
  CHECK(kind == D3dObjectKind::unordered_access_view);
  table.clear();
  CHECK_EQ(table.views(), 0u);
  table.clear(); }

TEST(d3d_object_table_note_viewed, remembers_what_a_view_slot_sees_until_the_slot_changes)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted view, resource;
  CHECK_STATUS(table.insert(9u, D3dObjectKind::shader_resource_view, &view), Status::ok);
  const std::uint32_t slot = table.view_slot(0u);
  CHECK(table.viewed(slot) == nullptr);
  table.note_viewed(slot, &resource);
  CHECK(table.viewed(slot) == &resource);
  table.note_viewed(D3dObjectTable::no_slot, &resource);   /* outside: nothing */
  CHECK(table.viewed(D3dObjectTable::no_slot) == nullptr);
  table.remove(9u);
  CHECK(table.viewed(slot) == nullptr);
  table.clear(); }

TEST(d3d_object_table_next_used, walks_the_used_slots_in_order)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  CHECK_EQ(table.next_used(0u), D3dObjectTable::no_slot);
  Counted objects[3];
  for (std::uint32_t id = 1u; id <= 3u; id++) CHECK_STATUS(table.insert(id * 100u, D3dObjectKind::fence12, &objects[id - 1u]), Status::ok);
  std::uint32_t seen = 0u;
  std::uint32_t previous = 0u;
  for (std::uint32_t slot = table.next_used(0u); slot != D3dObjectTable::no_slot; slot = table.next_used(slot + 1u)) {
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
  CHECK(table.at(D3dObjectTable::no_slot, id, kind) == nullptr);
  CHECK_EQ(id, 0u);
  CHECK(kind == D3dObjectKind::none);
  table.clear(); }

/* The old table refused past 196608 objects, three quarters of its
 * 262144 slots: Resident Evil 4 reached it a minute into its first area.
 * Three hundred thousand are kept, every one found as the kind it was made
 * as, and a removal in the middle leaves the rest where they were. */
TEST(d3d_object_table_insert, keeps_well_past_the_old_table)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  static Counted object;
  object.references = 1u;
  object.released = 0u;
  constexpr std::uint32_t many = 300000u;
  for (std::uint32_t id = 1u; id <= many; id++)
      CHECK_STATUS(table.insert(id, id % 2u == 0u ? D3dObjectKind::sampler : D3dObjectKind::shader_resource_view,
                                &object),
                   Status::ok);
  CHECK_EQ(table.count(), many);
  CHECK_EQ(table.views(), many / 2u);
  CHECK(table.invariants_hold());
  std::uint32_t found = 0u;
  for (std::uint32_t id = 1u; id <= many; id++)
      found += id % 2u == 0u ? table.find(id, D3dObjectKind::sampler) == &object : table.find_view(id) == &object;
  CHECK_EQ(found, many);
  table.remove(150001u);
  CHECK(table.find_view(150001u) == nullptr);
  CHECK(table.find_view(150003u) == &object);
  CHECK_EQ(table.views(), many / 2u - 1u);
  CHECK(table.invariants_hold());
  table.clear();
  CHECK_EQ(object.released, many);
  CHECK_EQ(platform.live_allocations(), 0u); }

/* A table that cannot grow refuses with no_memory and changes nothing: no
 * object, no view listed, the reference still the caller's - or, for keep,
 * given back. */
TEST(d3d_object_table_insert, a_table_that_cannot_grow_refuses_and_changes_nothing)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted        view, resource, made;
  platform.fail_allocation_at(1u);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::render_target_view, &view), Status::no_memory);
  CHECK_EQ(table.views(), 0u);
  CHECK(table.find_view(1u) == nullptr);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(table.insert(2u, D3dObjectKind::resource, &resource), Status::no_memory);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  platform.fail_allocation_at(2u);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::render_target_view, &view), Status::no_memory);
  CHECK_EQ(table.views(), 0u);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold());
  platform.fail_allocation_at(1u);
  CHECK_STATUS(table.keep(3u, D3dObjectKind::resource, &made), Status::no_memory);
  CHECK_EQ(made.released, 1u);
  CHECK_EQ(view.references, 1u);
  CHECK_EQ(resource.references, 1u);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::render_target_view, &view), Status::ok);
  CHECK_EQ(table.views(), 1u);
  CHECK(table.invariants_hold());
  table.clear(); }

/* What a create made is the table's, or let go: a null - a refused create -
 * is no object, and one the table cannot keep loses the reference it came
 * with rather than leaking it. */
TEST(d3d_object_table_keep, keeps_what_was_made_or_releases_it)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted made, twin;
  CHECK_STATUS(table.keep(5u, D3dObjectKind::resource, nullptr), Status::invalid_parameter);
  CHECK_STATUS(table.keep(5u, D3dObjectKind::resource, &made), Status::ok);
  CHECK_EQ(made.released, 0u);
  CHECK(table.find(5u, D3dObjectKind::resource) == &made);
  CHECK_STATUS(table.keep(5u, D3dObjectKind::resource, &twin), Status::conflict);
  CHECK_EQ(twin.released, 1u);
  CHECK(table.find(5u, D3dObjectKind::resource) == &made);
  CHECK(table.invariants_hold());
  table.clear(); }

TEST(d3d_object_table_find_as, answers_the_object_as_the_interface_its_kind_is_kept_as)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted buffer;
  CHECK_STATUS(table.insert(3u, D3dObjectKind::resource, &buffer), Status::ok);
  CHECK(table.find_as<Counted>(3u, D3dObjectKind::resource) == &buffer);
  CHECK(table.find_as<Counted>(3u, D3dObjectKind::shader_resource_view) == nullptr);
  table.clear(); }

/* One runtime's objects counted and let go together, the rest kept. */
TEST(d3d_object_table_kinds, count_and_remove_a_range_of_kinds_only)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  Counted resource, surface, query;
  CHECK_STATUS(table.insert(1u, D3dObjectKind::resource, &resource), Status::ok);
  CHECK_STATUS(table.insert(2u, D3dObjectKind::surface9, &surface), Status::ok);
  CHECK_STATUS(table.insert(3u, D3dObjectKind::query9, &query), Status::ok);
  CHECK_EQ(table.count_kinds(D3dObjectKind::surface9, D3dObjectKind::query9), 2u);
  CHECK_EQ(table.count_kinds(D3dObjectKind::heap12, D3dObjectKind::command_signature12), 0u);
  /* A range in the middle passes the kinds either side of it. */
  Counted heap;
  CHECK_STATUS(table.insert(4u, D3dObjectKind::heap12, &heap), Status::ok);
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

/* A table nothing was put in holds no memory and answers as an empty one;
 * a clear gives back every reference and all its memory. */
TEST(d3d_object_table_clear, an_empty_table_holds_nothing_and_a_clear_gives_everything_back)
{ MockPlatform   platform;
  D3dObjectTable table(platform);
  CHECK(table.find(1u, D3dObjectKind::resource) == nullptr);
  D3dObjectKind kind = D3dObjectKind::resource;
  CHECK(table.find_any(1u, kind) == nullptr);
  CHECK(kind == D3dObjectKind::none);
  Counted object, view;
  CHECK(table.exchange(1u, D3dObjectKind::resource, &object) == nullptr);
  CHECK_EQ(table.next_used(0u), D3dObjectTable::no_slot);
  CHECK_EQ(table.view_slot(0u), D3dObjectTable::no_slot);
  table.note_viewed(0u, &object);
  CHECK(table.viewed(0u) == nullptr);
  std::uint32_t id = 9u;
  CHECK(table.at(0u, id, kind) == nullptr);
  CHECK_EQ(id, 0u);
  table.remove(1u);
  table.remove_kinds(D3dObjectKind::resource, D3dObjectKind::query9);
  CHECK_EQ(table.count_kinds(D3dObjectKind::resource, D3dObjectKind::query9), 0u);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK_STATUS(table.insert(1u, D3dObjectKind::resource, &object), Status::ok);
  CHECK_STATUS(table.insert(2u, D3dObjectKind::shader_resource_view, &view), Status::ok);
  table.note_viewed(table.view_slot(0u), &object);
  table.clear();
  CHECK_EQ(object.released, 1u);
  CHECK_EQ(view.released, 1u);
  CHECK_EQ(table.views(), 0u);
  CHECK_EQ(table.count(), 0u);
  CHECK_EQ(table.next_used(0u), D3dObjectTable::no_slot);
  CHECK_EQ(platform.live_allocations(), 0u);
  CHECK(table.invariants_hold()); }

/* Each way the table could disagree with itself fails the invariant: a view
 * whose place is wrong, a slot listed that is not a view, a view not listed,
 * an entry with no object. */
TEST(d3d_object_table_invariants, catch_a_view_list_or_an_entry_out_of_step)
{ for (int knob = 0; knob < 5; knob++) {
      MockPlatform   platform;
      D3dObjectTable table(platform);
      Counted        resource, first, second;
      CHECK_STATUS(table.insert(1u, D3dObjectKind::resource, &resource), Status::ok);
      CHECK_STATUS(table.insert(2u, D3dObjectKind::shader_resource_view, &first), Status::ok);
      CHECK_STATUS(table.insert(3u, D3dObjectKind::render_target_view, &second), Status::ok);
      CHECK(table.invariants_hold());
      if (knob == 0) D3dObjectTableProbe::misplace_view(table, 2u, 1u);
      if (knob == 1) D3dObjectTableProbe::misplace_view(table, 3u, 7u);
      if (knob == 2) D3dObjectTableProbe::list_slot(table, 0u);
      if (knob == 3) D3dObjectTableProbe::unlist_views(table);
      if (knob == 4) D3dObjectTableProbe::drop_object(table, 1u);
      CHECK(!table.invariants_hold());
  } }
