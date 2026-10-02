/* The blobs one session holds, by the id the guest used. */
#include "device/abi/av_executor.h"
#include "executor/import_table.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ImportedBlob;
using av::ImportTable;
using av::Status;

namespace {

std::uint8_t backing[ImportedBlob::page_bytes];

AvBlobHandle handle_of(std::uint64_t size = ImportedBlob::page_bytes)
{ AvBlobHandle handle = {};
  handle.kind = AV_BLOB_HOST_POINTER;
  handle.pointer = backing;
  handle.fd = -1;
  handle.size = size;
  return handle; }

}  // namespace

TEST(import_table, holds_a_blob_under_the_id_the_guest_used)
{ ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, handle_of(), taken), Status::ok);
  CHECK(taken != nullptr);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(9u) == taken);
  CHECK(table.find(10u) == nullptr);
  CHECK(table.invariants_hold()); }

/* A re-import of a live id is refused rather than silently tearing the old one
 * down, which is what a hostile guest would drive at the worst moment. */
TEST(import_table, an_id_already_imported_is_a_conflict)
{ ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, handle_of(), taken), Status::ok);
  ImportedBlob *again = taken;
  CHECK_STATUS(table.take(9u, handle_of(), again), Status::conflict);
  CHECK(again == nullptr);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(9u) == taken); }

/* Release is idempotent: the device may release an id whose import failed. */
TEST(import_table, dropping_one_it_never_had_is_nothing)
{ ImportTable table;
  CHECK(table.drop(9u) == nullptr);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }

TEST(import_table, a_dropped_slot_comes_back)
{ ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, handle_of(), taken), Status::ok);
  ImportedBlob *const dropped = table.drop(9u);
  CHECK(dropped == taken);
  /* The caller gives it back to the host API and then to the table. */
  dropped->give_back();
  CHECK_EQ(table.count(), 0u);
  CHECK(table.find(9u) == nullptr);
  CHECK_STATUS(table.take(9u, handle_of(), taken), Status::ok);
  CHECK(table.invariants_hold()); }

/* A handle that breaks the ABI's rules never reaches a host API, and never
 * costs a slot. */
TEST(import_table, a_handle_the_abi_refuses_costs_no_slot)
{ ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, handle_of(1u), taken), Status::invalid_parameter);
  CHECK(taken == nullptr);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }

/* A guest that imports more than a context may hold is refused, not served. */
TEST(import_table, a_table_that_is_full_refuses_the_next)
{ ImportTable table;
  ImportedBlob *taken = nullptr;
  for (std::uint32_t i = 0u; i < ImportTable::max_imports; i++) {
      CHECK_STATUS(table.take(i + 1u, handle_of(), taken), Status::ok);
  }
  CHECK_EQ(table.count(), ImportTable::max_imports);
  CHECK_STATUS(table.take(0xFFFFu, handle_of(), taken), Status::no_space);
  CHECK(taken == nullptr);
  CHECK(table.invariants_hold()); }

/* A session being destroyed releases what the guest never did. */
TEST(import_table, offers_every_blob_still_held_so_none_is_left_behind)
{ ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK(table.any() == nullptr);
  for (std::uint32_t i = 0u; i < 4u; i++) CHECK_STATUS(table.take(i + 1u, handle_of(), taken), Status::ok);

  std::uint32_t released = 0u;
  for (ImportedBlob *blob = table.any(); blob != nullptr; blob = table.any()) {
      CHECK(table.drop(blob->id()) == blob);
      blob->give_back();
      released++;
  }
  CHECK_EQ(released, 4u);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }
