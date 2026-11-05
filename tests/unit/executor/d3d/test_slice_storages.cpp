#include "core/support/status.hpp"
#include "executor/d3d/slice_storages.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/scripted_slice_importer.hpp"
#include "harness/executor/scripted_slice_progress.hpp"
#include "harness/executor/slice_store_rig.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ScriptedSliceImporter;
using av::ScriptedSliceProgress;
using av::SliceBindings;
using av::SliceStorages;
using av::SliceStoreRig;
using av::SourceMemory;
using av::Status;
using av::VulkanBuffer;

namespace {

std::uint8_t bytes[64];

class Rig {
public:
    Rig() : storages_(stores_.platform(), stores_.lines()) {}
    ~Rig() { storages_.destroy_all(); }

    SliceStoreRig         stores_;
    SliceStorages         storages_;
    ScriptedSliceImporter importer_;
    ScriptedSliceImporter other_;
    ScriptedSliceProgress progress_;
};

SliceBindings::Binding binding(std::uint32_t resource, const SourceMemory *store, const av::SliceImporter *importer)
{
    return SliceBindings::Binding{resource, store, bytes + resource % 32u, 64u, importer};
}

}

TEST(slice_storages, starts_empty)
{ Rig rig;
  CHECK_EQ(rig.storages_.count(), 0u);
  CHECK(!rig.storages_.holds(*rig.stores_.store(1u)));
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bind, imports_a_store_once_for_an_importer_and_answers_the_same_buffer_after)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  const VulkanBuffer *const first = rig.storages_.bind(rig.importer_, rig.progress_, store);
  CHECK(first != nullptr);
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.holds(store));
  CHECK_EQ(rig.importer_.last_bytes(), store.size());
  const VulkanBuffer *const again = rig.storages_.bind(rig.importer_, rig.progress_, store);
  CHECK(again == first);
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bind, each_importer_gets_its_own_storage_of_the_same_store)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  const VulkanBuffer *const one = rig.storages_.bind(rig.importer_, rig.progress_, store);
  const VulkanBuffer *const two = rig.storages_.bind(rig.other_, rig.progress_, store);
  CHECK(one != nullptr && two != nullptr && one != two);
  CHECK_EQ(rig.storages_.count(), 2u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bind, a_refused_import_answers_nothing_and_is_never_asked_again)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  rig.importer_.refuse(true);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) == nullptr);
  CHECK_EQ(rig.importer_.imports(), 1u);
  rig.importer_.refuse(false);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) == nullptr);
  CHECK_EQ(rig.importer_.imports(), 1u);
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bind, a_refusal_is_reported_once_however_many_stores_decline)
{ Rig rig;
  rig.importer_.refuse(true);
  for (std::uint64_t key = 1u; key <= 3u; key++)
      CHECK(rig.storages_.bind(rig.importer_, rig.progress_, *rig.stores_.store(key)) == nullptr);
  CHECK_EQ(rig.stores_.sink().lines(), 1u);
  CHECK(rig.stores_.sink().holding("could not be imported to draw from") == 1u); }

TEST(slice_storages_bind, a_refusal_is_reported_again_after_everything_was_destroyed)
{ Rig rig;
  rig.importer_.refuse(true);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, *rig.stores_.store(1u)) == nullptr);
  rig.storages_.destroy_all();
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, *rig.stores_.store(2u)) == nullptr);
  CHECK_EQ(rig.stores_.sink().lines(), 2u); }

TEST(slice_storages_bind, a_retired_store_is_not_bound_again_for_the_importer_that_retired_it)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  rig.storages_.retire(store);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) == nullptr);
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.bind(rig.other_, rig.progress_, store) != nullptr);
  CHECK_EQ(rig.storages_.count(), 2u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bind, holds_a_reference_to_the_store_per_storage)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  const std::uint32_t before = store.references();
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  CHECK(rig.storages_.bind(rig.other_, rig.progress_, store) != nullptr);
  CHECK_EQ(store.references(), before + 2u);
  rig.storages_.destroy_all();
  CHECK_EQ(store.references(), before); }

TEST(slice_storages_retire, marks_every_live_storage_of_the_store_and_leaves_the_others)
{ Rig rig;
  SourceMemory &one = *rig.stores_.store(1u);
  SourceMemory &two = *rig.stores_.store(2u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, one) != nullptr);
  CHECK(rig.storages_.bind(rig.other_, rig.progress_, one) != nullptr);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, two) != nullptr);
  rig.storages_.retire(one);
  CHECK(rig.storages_.invariants_hold());
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, two) != nullptr);
  CHECK_EQ(rig.storages_.count(), 3u);
  rig.storages_.retire(one);
  CHECK(rig.storages_.invariants_hold());
  rig.storages_.retire(*rig.stores_.store(3u));
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_reclaim, destroys_a_retired_storage_only_after_the_gpu_reached_its_last_use)
{ Rig rig;
  rig.progress_.set(5u, 0u);
  SourceMemory &store = *rig.stores_.store(1u);
  const std::uint32_t before = store.references();
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  rig.storages_.retire(store);
  rig.storages_.reclaim();
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK_EQ(store.references(), before + 1u);
  rig.progress_.set(6u, 4u);
  rig.storages_.reclaim();
  CHECK_EQ(rig.storages_.count(), 1u);
  rig.progress_.set(6u, 5u);
  rig.storages_.reclaim();
  CHECK_EQ(rig.storages_.count(), 0u);
  CHECK(!rig.storages_.holds(store));
  CHECK_EQ(store.references(), before);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_reclaim, a_storage_still_in_use_is_never_reclaimed)
{ Rig rig;
  rig.progress_.set(5u, 0u);
  SourceMemory &store = *rig.stores_.store(1u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  rig.progress_.set(6u, 1000u);
  rig.storages_.reclaim();
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_reclaim, every_bind_reclaims_the_finished_first)
{ Rig rig;
  rig.progress_.set(5u, 0u);
  SourceMemory &old = *rig.stores_.store(1u);
  SourceMemory &fresh = *rig.stores_.store(2u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, old) != nullptr);
  rig.storages_.retire(old);
  rig.progress_.set(6u, 5u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, fresh) != nullptr);
  CHECK(!rig.storages_.holds(old));
  CHECK(rig.storages_.holds(fresh));
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_reclaim, a_stamp_after_the_retire_cannot_be_missed)
{ Rig rig;
  rig.progress_.set(5u, 0u);
  SourceMemory &store = *rig.stores_.store(1u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  rig.progress_.set(9u, 0u);
  rig.storages_.retire(store);
  rig.progress_.set(9u, 8u);
  rig.storages_.reclaim();
  CHECK_EQ(rig.storages_.count(), 1u);
  rig.progress_.set(9u, 9u);
  rig.storages_.reclaim();
  CHECK_EQ(rig.storages_.count(), 0u); }

TEST(slice_storages_destroy_for, removes_one_importers_storages_and_bindings_and_no_one_elses)
{ Rig rig;
  SourceMemory &one = *rig.stores_.store(1u);
  SourceMemory &two = *rig.stores_.store(2u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, one) != nullptr);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, two) != nullptr);
  CHECK(rig.storages_.bind(rig.other_, rig.progress_, one) != nullptr);
  CHECK_STATUS(rig.storages_.attach(binding(1u, &one, &rig.importer_)), Status::ok);
  CHECK_STATUS(rig.storages_.attach(binding(2u, &one, &rig.other_)), Status::ok);
  rig.storages_.retire(two);
  rig.storages_.destroy_for(rig.importer_);
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.holds(one));
  CHECK(!rig.storages_.holds(two));
  SliceBindings::Binding was = {};
  CHECK(!rig.storages_.bound(1u, was));
  CHECK(rig.storages_.bound(2u, was));
  CHECK(rig.storages_.invariants_hold());
  rig.storages_.destroy_for(rig.importer_);
  CHECK_EQ(rig.storages_.count(), 1u);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_destroy_for, a_retired_storage_destroyed_this_way_no_longer_counts_as_retiring)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  rig.storages_.retire(store);
  rig.storages_.destroy_for(rig.importer_);
  CHECK_EQ(rig.storages_.count(), 0u);
  CHECK(rig.storages_.invariants_hold());
  rig.progress_.set(100u, 100u);
  rig.storages_.reclaim();
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bindings, attach_leave_and_bound_keep_a_resource_to_its_slice)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  CHECK_STATUS(rig.storages_.attach(binding(7u, &store, &rig.importer_)), Status::ok);
  SliceBindings::Binding was = {};
  CHECK(rig.storages_.bound(7u, was));
  CHECK(was.store_ == &store);
  CHECK(rig.storages_.leave(7u, was));
  CHECK_EQ(was.resource_, 7u);
  CHECK(!rig.storages_.bound(7u, was));
  CHECK(!rig.storages_.leave(7u, was));
  SliceBindings::Binding bad = binding(8u, nullptr, &rig.importer_);
  CHECK_STATUS(rig.storages_.attach(bad), Status::invalid_parameter);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bindings, take_all_bound_to_visits_every_resource_on_a_store_and_forgets_them)
{ Rig rig;
  SourceMemory &one = *rig.stores_.store(1u);
  SourceMemory &two = *rig.stores_.store(2u);
  for (std::uint32_t resource = 1u; resource <= 6u; resource++)
      CHECK_STATUS(rig.storages_.attach(binding(resource, resource % 2u != 0u ? &one : &two, &rig.importer_)), Status::ok);
  std::uint32_t visited = 0u;
  rig.storages_.take_all_bound_to(one, [&](const SliceBindings::Binding &was) {
      CHECK(was.store_ == &one);
      visited++;
  });
  CHECK_EQ(visited, 3u);
  SliceBindings::Binding was = {};
  CHECK(!rig.storages_.bound(1u, was));
  CHECK(rig.storages_.bound(2u, was));
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_destroy_all, drops_storages_and_bindings_and_every_reference)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  const std::uint32_t before = store.references();
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  rig.storages_.retire(store);
  CHECK_STATUS(rig.storages_.attach(binding(3u, &store, &rig.importer_)), Status::ok);
  rig.storages_.destroy_all();
  CHECK_EQ(rig.storages_.count(), 0u);
  CHECK_EQ(store.references(), before);
  SliceBindings::Binding was = {};
  CHECK(!rig.storages_.bound(3u, was));
  CHECK(rig.storages_.invariants_hold());
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr);
  CHECK(rig.storages_.invariants_hold()); }

TEST(slice_storages_bind, an_allocation_failure_answers_nothing_and_leaves_no_storage)
{ Rig rig;
  SourceMemory &store = *rig.stores_.store(1u);
  rig.stores_.platform().fail_allocation_at(1u);
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) == nullptr);
  CHECK_EQ(rig.storages_.count(), 0u);
  CHECK(rig.storages_.invariants_hold());
  CHECK(rig.storages_.bind(rig.importer_, rig.progress_, store) != nullptr); }
