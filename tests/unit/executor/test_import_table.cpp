/* The imports one session holds, by the id the device chose for each. */
#include "executor/exported_memory.hpp"
#include "executor/import_table.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/imported_blob_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExportedMemory;
using av::ImportedBlob;
using av::ImportTable;
using av::SourceMemory;
using av::Status;

namespace {

std::uint8_t backing[4096];

class Memory {
public:
    Memory() : memory(1u, 0x20u, 0u, 1u, ExportedMemory(0x55u, -1, backing, sizeof backing)) {}

    SourceMemory memory;
};

}  // namespace

TEST(import_table, holds_memory_under_the_id_the_device_chose)
{ Memory      held;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(taken != nullptr);
  CHECK(&taken->memory() == &held.memory);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(9u) == taken);
  CHECK(table.find(10u) == nullptr);
  CHECK(table.invariants_hold()); }

/* A re-import of a live id is refused rather than silently tearing the old one
 * down, which is what a hostile guest would drive at the worst moment. */
TEST(import_table, an_id_already_imported_is_a_conflict)
{ Memory      first;
  Memory      second;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, first.memory, taken), Status::ok);
  ImportedBlob *again = taken;
  CHECK_STATUS(table.take(9u, second.memory, again), Status::conflict);
  CHECK(again == nullptr);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(9u) == taken);
  CHECK(&table.find(9u)->memory() == &first.memory);
  CHECK(table.invariants_hold()); }

/* Release is idempotent: the device may release an id whose import failed. */
TEST(import_table, dropping_one_it_never_had_is_nothing)
{ ImportTable table;
  CHECK(table.drop(9u) == nullptr);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }

TEST(import_table, a_dropped_slot_comes_back)
{ Memory      held;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  ImportedBlob *const dropped = table.drop(9u);
  CHECK(dropped == taken);
  /* The caller gives it back to the host API and then to the table. */
  CHECK(dropped->give_back() == &held.memory);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.find(9u) == nullptr);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(taken == dropped);
  CHECK(table.invariants_hold()); }

/* Between drop and give_back the slot is still the import's: a table whose
 * count and slots disagree is one mid-release, which its invariant says. */
TEST(import_table_invariants, a_slot_dropped_and_not_given_back_breaks_them)
{ Memory      held;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(table.drop(9u) == taken);
  CHECK(!table.invariants_hold());
  static_cast<void>(taken->give_back());
  CHECK(table.invariants_hold()); }

/* The table answers from its slots, so one slot that breaks its own breaks
 * the table: a free slot still naming an import. */
TEST(import_table_invariants, a_slot_that_breaks_its_own_breaks_them)
{ Memory      held;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(table.drop(9u) == taken);
  static_cast<void>(taken->give_back());
  CHECK(table.invariants_hold());
  av::ImportedBlobProbe(*taken).set_id(9u);
  CHECK(!table.invariants_hold()); }

/* A guest that imports more than a context may hold is refused, not served. */
TEST(import_table, a_table_that_is_full_refuses_the_next)
{ Memory      held;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  for (std::uint32_t i = 0u; i < ImportTable::max_imports; i++)
      CHECK_STATUS(table.take(i + 1u, held.memory, taken), Status::ok);
  CHECK_EQ(table.count(), ImportTable::max_imports);
  CHECK_STATUS(table.take(0xFFFFu, held.memory, taken), Status::no_space);
  CHECK(taken == nullptr);
  CHECK(table.find(0xFFFFu) == nullptr);
  CHECK(table.invariants_hold()); }

/* A session being destroyed releases what the guest never did. */
TEST(import_table, offers_every_import_still_held_so_none_is_left_behind)
{ Memory      held;
  ImportTable table;
  ImportedBlob *taken = nullptr;
  CHECK(table.any() == nullptr);
  for (std::uint32_t i = 0u; i < 4u; i++) CHECK_STATUS(table.take(i + 1u, held.memory, taken), Status::ok);

  std::uint32_t released = 0u;
  for (ImportedBlob *blob = table.any(); blob != nullptr; blob = table.any()) {
      CHECK(table.drop(blob->id()) == blob);
      static_cast<void>(blob->give_back());
      released++;
  }
  CHECK_EQ(released, 4u);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }
