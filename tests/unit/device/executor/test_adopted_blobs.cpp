/* The blobs a Vulkan backend made memory of, found by that memory. What a
 * caller acts on is which blob a freed handle names - the wrong one, or one
 * already given back, is an import released twice. */
#include "core/support/status.hpp"
#include "device/abi/rvk_executor.h"
#include "device/executor/adopted_blobs.hpp"
#include "device/executor/imported_blob.hpp"
#include "harness/executor/adopted_blobs_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::AdoptedBlobs;
using av::AdoptedBlobsProbe;
using av::ImportedBlob;
using av::Status;

namespace {

std::uint8_t backing[ImportedBlob::page_bytes];

/* A blob taken and holding `object`, as a backend leaves one it adopted. */
void adopted(ImportedBlob &blob, std::uint64_t id, std::uint64_t object)
{ RvkBlobHandle handle = {};
  handle.kind = RVK_BLOB_HOST_POINTER;
  handle.pointer = backing;
  handle.fd = -1;
  handle.size = sizeof backing;
  CHECK_STATUS(blob.take(id, handle), Status::ok);
  blob.record(object, object); }

}  // namespace

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
  /* Null memory is what an empty blob holds, so it names none of them. */
  CHECK(adopted_blobs.find(0u) == nullptr); }

/* A blob holding nothing, one already here, or another blob naming the same
 * memory would each let one free release something twice. */
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

/* Full is a refusal, and a removal makes the room again. */
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

/* Removing one leaves the others found, removing it again or one never added
 * is nothing, and clearing forgets all of them. */
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

/* Past the room, an empty entry, a blob given back or holding nothing, and one
 * blob twice: each is a free that finds the wrong blob or none. */
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
