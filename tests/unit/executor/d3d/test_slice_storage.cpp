#include "executor/d3d/slice_storage.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/scripted_slice_importer.hpp"
#include "harness/executor/scripted_slice_progress.hpp"
#include "harness/executor/slice_store_rig.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ScriptedSliceImporter;
using av::ScriptedSliceProgress;
using av::SliceStoreRig;
using av::SliceStorage;
using av::SourceMemory;

TEST(slice_storage, holds_a_reference_to_its_store_for_as_long_as_it_lives)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress;
  const std::uint32_t   before = store.references();
  {
      SliceStorage storage(store, importer, progress);
      CHECK_EQ(store.references(), before + 1u);
      CHECK(storage.invariants_hold());
  }
  CHECK_EQ(store.references(), before); }

TEST(slice_storage, starts_unimported_unrefused_and_live)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress;
  SliceStorage          storage(store, importer, progress);
  CHECK(!storage.imported());
  CHECK(!storage.refused());
  CHECK(!storage.retired());
  CHECK(!storage.done());
  CHECK(!storage.buffer().made()); }

TEST(slice_storage, knows_which_store_and_importer_it_serves)
{ SliceStoreRig        rig;
  SourceMemory        &one = *rig.store(1u);
  SourceMemory        &two = *rig.store(2u);
  ScriptedSliceImporter importer;
  ScriptedSliceImporter other;
  ScriptedSliceProgress progress;
  SliceStorage          storage(one, importer, progress);
  CHECK(storage.holds(one, importer));
  CHECK(!storage.holds(two, importer));
  CHECK(!storage.holds(one, other));
  CHECK(storage.holds_memory(one));
  CHECK(!storage.holds_memory(two));
  CHECK(storage.imported_by(importer));
  CHECK(!storage.imported_by(other)); }

TEST(slice_storage_import, hands_the_importer_a_shared_descriptor_and_the_whole_stores_size)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress;
  SliceStorage          storage(store, importer, progress);
  CHECK(storage.import());
  CHECK_EQ(importer.imports(), 1u);
  CHECK_EQ(importer.last_bytes(), store.size());
  CHECK(importer.last_descriptor_held());
  CHECK(!storage.refused());
  CHECK(storage.invariants_hold()); }

TEST(slice_storage_import, an_importer_that_declines_marks_the_storage_refused)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress;
  importer.refuse(true);
  SliceStorage storage(store, importer, progress);
  CHECK(!storage.import());
  CHECK(storage.refused());
  CHECK(!storage.imported());
  CHECK(storage.invariants_hold()); }

TEST(slice_storage_stamp, remembers_the_newest_submission_and_never_goes_back)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress(5u, 0u);
  SliceStorage          storage(store, importer, progress);
  storage.stamp();
  storage.retire();
  progress.set(2u, 4u);
  storage.stamp();
  CHECK(!storage.done());
  progress.set(2u, 5u);
  CHECK(storage.done()); }

TEST(slice_storage_retire, a_retired_storage_is_done_only_once_the_gpu_reached_its_stamp)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress(10u, 0u);
  SliceStorage          storage(store, importer, progress);
  storage.stamp();
  storage.retire();
  CHECK(storage.retired());
  CHECK(!storage.done());
  progress.set(11u, 9u);
  CHECK(!storage.done());
  progress.set(11u, 10u);
  CHECK(storage.done());
  progress.set(11u, 1000u);
  CHECK(storage.done()); }

TEST(slice_storage_retire, a_storage_not_retired_is_never_done_however_far_the_gpu_is)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress(10u, 1000u);
  SliceStorage          storage(store, importer, progress);
  storage.stamp();
  CHECK(!storage.done()); }

TEST(slice_storage_retire, a_storage_never_stamped_is_done_at_once_when_retired)
{ SliceStoreRig        rig;
  SourceMemory        &store = *rig.store(1u);
  ScriptedSliceImporter importer;
  ScriptedSliceProgress progress(10u, 0u);
  SliceStorage          storage(store, importer, progress);
  storage.retire();
  CHECK(storage.done()); }
