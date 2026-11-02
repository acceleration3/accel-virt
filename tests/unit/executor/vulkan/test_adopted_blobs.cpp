#include "core/support/status.hpp"
#include "executor/exported_memory.hpp"
#include "executor/source_memory.hpp"
#include "executor/vulkan/adopted_blobs.hpp"
#include "executor/imported_blob.hpp"
#include "harness/executor/adopted_blobs_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::AdoptedBlobs;
using av::AdoptedBlobsProbe;
using av::ImportedBlob;
using av::Status;

namespace {

std::uint8_t backing[4096];

av::SourceMemory &memory()
{ static av::SourceMemory shared(1u, 1u, 0u, 1u, av::ExportedMemory(0x10u, -1, backing, sizeof backing));
  return shared; }

void adopted(ImportedBlob &blob, std::uint64_t id, std::uint64_t object)
{ CHECK_STATUS(blob.take(id, memory()), Status::ok);
  blob.record(object, object); }

}

TEST(adopted_blobs, finds_a_blob_by_the_memory_it_holds)
{ AdoptedBlobs adopted_blobs;
  ImportedBlob first;
  ImportedBlob second;
  adopted(first, 1u, 0x1000u);
  adopted(second, 2u, 0x2000u);
  CHECK_STATUS(adopted_blobs.add(first), Status::ok);
  CHECK_STATUS(adopted_blobs.add(second), Status::ok);
  CHECK(adopted_blobs.invariants_hold());
  CHECK_EQ(adopted_blobs.count(), 2u);
  CHECK(adopted_blobs.find(0x1000u) == &first);
  CHECK(adopted_blobs.find(0x2000u) == &second);
  CHECK(adopted_blobs.find(0x3000u) == nullptr);
  CHECK(adopted_blobs.find(0u) == nullptr); }

TEST(adopted_blobs, refuses_a_blob_it_could_not_tell_apart)
{ AdoptedBlobs adopted_blobs;
  ImportedBlob empty;
  CHECK_STATUS(adopted_blobs.add(empty), Status::invalid_parameter);
  ImportedBlob unmade;
  adopted(unmade, 1u, 0u);
  CHECK_STATUS(adopted_blobs.add(unmade), Status::invalid_parameter);

  ImportedBlob held;
  adopted(held, 2u, 0x1000u);
  CHECK_STATUS(adopted_blobs.add(held), Status::ok);
  CHECK_STATUS(adopted_blobs.add(held), Status::conflict);
  ImportedBlob twin;
  adopted(twin, 3u, 0x1000u);
  CHECK_STATUS(adopted_blobs.add(twin), Status::conflict);
  CHECK_EQ(adopted_blobs.count(), 1u);
  CHECK(adopted_blobs.invariants_hold()); }

TEST(adopted_blobs, a_full_table_refuses_the_next_until_one_is_removed)
{ static ImportedBlob blobs[AdoptedBlobs::capacity + 1u];
  AdoptedBlobs adopted_blobs;
  for (std::uint32_t i = 0u; i <= AdoptedBlobs::capacity; i++)
      adopted(blobs[i], i + 1u, 0x1000u * (i + 1u));
  for (std::uint32_t i = 0u; i < AdoptedBlobs::capacity; i++)
      CHECK_STATUS(adopted_blobs.add(blobs[i]), Status::ok);
  CHECK_STATUS(adopted_blobs.add(blobs[AdoptedBlobs::capacity]), Status::no_space);
  CHECK(adopted_blobs.invariants_hold());
  adopted_blobs.remove(blobs[0]);
  CHECK_STATUS(adopted_blobs.add(blobs[AdoptedBlobs::capacity]), Status::ok);
  CHECK_EQ(adopted_blobs.count(), AdoptedBlobs::capacity);
  CHECK(adopted_blobs.invariants_hold()); }

TEST(adopted_blobs, removing_forgets_one_and_clearing_forgets_all)
{ AdoptedBlobs adopted_blobs;
  ImportedBlob first;
  ImportedBlob second;
  ImportedBlob third;
  ImportedBlob stranger;
  adopted(first, 1u, 0x1000u);
  adopted(second, 2u, 0x2000u);
  adopted(third, 3u, 0x3000u);
  adopted(stranger, 4u, 0x4000u);
  CHECK_STATUS(adopted_blobs.add(first), Status::ok);
  CHECK_STATUS(adopted_blobs.add(second), Status::ok);
  CHECK_STATUS(adopted_blobs.add(third), Status::ok);

  adopted_blobs.remove(first);
  CHECK(adopted_blobs.invariants_hold());
  CHECK_EQ(adopted_blobs.count(), 2u);
  CHECK(adopted_blobs.find(0x1000u) == nullptr);
  CHECK(adopted_blobs.find(0x2000u) == &second);
  CHECK(adopted_blobs.find(0x3000u) == &third);
  adopted_blobs.remove(first);
  adopted_blobs.remove(stranger);
  CHECK_EQ(adopted_blobs.count(), 2u);

  adopted_blobs.clear();
  CHECK_EQ(adopted_blobs.count(), 0u);
  CHECK(adopted_blobs.find(0x2000u) == nullptr);
  CHECK(adopted_blobs.invariants_hold()); }

TEST(adopted_blobs_invariants, a_count_past_the_room_breaks_them)
{ AdoptedBlobs adopted_blobs;
  AdoptedBlobsProbe(adopted_blobs).set_count(AdoptedBlobs::capacity + 1u);
  CHECK(!adopted_blobs.invariants_hold()); }

TEST(adopted_blobs_invariants, an_empty_entry_breaks_them)
{ AdoptedBlobs adopted_blobs;
  AdoptedBlobsProbe(adopted_blobs).set_count(1u);
  CHECK(!adopted_blobs.invariants_hold()); }

TEST(adopted_blobs_invariants, a_blob_given_back_breaks_them)
{ AdoptedBlobs adopted_blobs;
  ImportedBlob blob;
  adopted(blob, 1u, 0x1000u);
  CHECK_STATUS(adopted_blobs.add(blob), Status::ok);
  blob.give_back();
  CHECK(!adopted_blobs.invariants_hold()); }

TEST(adopted_blobs_invariants, a_blob_holding_nothing_breaks_them)
{ AdoptedBlobs adopted_blobs;
  ImportedBlob blob;
  adopted(blob, 1u, 0x1000u);
  CHECK_STATUS(adopted_blobs.add(blob), Status::ok);
  blob.record(0u, 0u);
  CHECK(!adopted_blobs.invariants_hold()); }

TEST(adopted_blobs_invariants, one_blob_twice_breaks_them)
{ AdoptedBlobs adopted_blobs;
  ImportedBlob first;
  ImportedBlob second;
  adopted(first, 1u, 0x1000u);
  adopted(second, 2u, 0x2000u);
  CHECK_STATUS(adopted_blobs.add(first), Status::ok);
  CHECK_STATUS(adopted_blobs.add(second), Status::ok);
  CHECK(adopted_blobs.invariants_hold());
  AdoptedBlobsProbe(adopted_blobs).set_blob(1u, &first);
  CHECK(!adopted_blobs.invariants_hold()); }
