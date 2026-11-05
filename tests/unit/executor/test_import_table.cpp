#include "executor/exported_memory.hpp"
#include "executor/import_table.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/imported_blob_probe.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"

#include <cstdint>

using av::ExportedMemory;
using av::ImportedBlob;
using av::ImportTable;
using av::MockPlatform;
using av::SourceMemory;
using av::Status;

namespace {

std::uint8_t backing[4096];

class Memory {
public:
    Memory() : memory(1u, 0x20u, 0u, 1u, ExportedMemory(0x55u, -1, backing, sizeof backing)) {}

    SourceMemory memory;
};

}

TEST(import_table, holds_memory_under_the_id_the_device_chose)
{ Memory      held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(taken != nullptr);
  CHECK(&taken->memory() == &held.memory);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(9u) == taken);
  CHECK(table.find(10u) == nullptr);
  CHECK(table.invariants_hold()); }

TEST(import_table, an_id_already_imported_is_a_conflict)
{ Memory      first;
  Memory      second;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, first.memory, taken), Status::ok);
  ImportedBlob *again = taken;
  CHECK_STATUS(table.take(9u, second.memory, again), Status::conflict);
  CHECK(again == nullptr);
  CHECK_EQ(table.count(), 1u);
  CHECK(table.find(9u) == taken);
  CHECK(&table.find(9u)->memory() == &first.memory);
  CHECK(table.invariants_hold()); }

TEST(import_table, forgetting_one_it_never_had_is_nothing)
{ MockPlatform platform;
  ImportTable  table(platform);
  CHECK(table.forget(9u) == nullptr);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }

TEST(import_table, a_forgotten_import_gives_its_memory_back_and_its_id_comes_back)
{ Memory       held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(table.forget(9u) == &held.memory);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.find(9u) == nullptr);
  CHECK(table.forget(9u) == nullptr);
  CHECK(table.invariants_hold());
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK(table.find(9u) == taken);
  CHECK(table.invariants_hold()); }

TEST(import_table_invariants, a_blob_held_under_an_id_it_does_not_hold_breaks_them)
{ Memory       held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  av::ImportedBlobProbe(*taken).set_id(10u);
  CHECK(!table.invariants_hold());
  av::ImportedBlobProbe(*taken).set_id(9u);
  CHECK(table.invariants_hold()); }

TEST(import_table, holds_imports_past_any_old_bound_and_refuses_only_when_it_cannot_grow)
{ Memory       held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  ImportedBlob *first = nullptr;
  constexpr std::uint32_t many = 6000u;
  for (std::uint32_t i = 0u; i < many; i++) {
      CHECK_STATUS(table.take(i + 1u, held.memory, taken), Status::ok);
      if (i == 0u) first = taken;
  }
  CHECK_EQ(table.count(), many);
  CHECK(table.find(1u) == first);
  for (std::uint32_t i = 0u; i < many; i++) CHECK(table.find(i + 1u)->holds(i + 1u));
  CHECK(table.invariants_hold());
  platform.fail_allocation_at(1u);
  std::uint64_t next = many + 1u;
  Status        refused = Status::ok;
  while ((refused = table.take(next, held.memory, taken)) == Status::ok) next++;
  CHECK_STATUS(refused, Status::no_memory);
  CHECK(taken == nullptr);
  CHECK(table.find(next) == nullptr);
  CHECK(table.invariants_hold());
  for (ImportedBlob *blob = table.any(); blob != nullptr; blob = table.any()) CHECK(table.forget(blob->id()) != nullptr);
  CHECK_EQ(table.count(), 0u); }

TEST(import_table, a_blob_that_cannot_be_made_leaves_no_id)
{ Memory       held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  platform.fail_allocation_at(3u);
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::no_memory);
  CHECK(taken == nullptr);
  CHECK(table.find(9u) == nullptr);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }

TEST(import_table, offers_every_import_still_held_so_none_is_left_behind)
{ Memory       held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  CHECK(table.any() == nullptr);
  for (std::uint32_t i = 0u; i < 4u; i++) CHECK_STATUS(table.take(i + 1u, held.memory, taken), Status::ok);
  std::uint32_t released = 0u;
  for (ImportedBlob *blob = table.any(); blob != nullptr; blob = table.any()) {
      CHECK(table.forget(blob->id()) == &held.memory);
      released++;
  }
  CHECK_EQ(released, 4u);
  CHECK_EQ(table.count(), 0u);
  CHECK(table.invariants_hold()); }

TEST(import_table_unname, forgets_the_handle_a_blob_was_named_by_and_nothing_else)
{ Memory       held;
  MockPlatform platform;
  ImportTable  table(platform);
  ImportedBlob *taken = nullptr;
  CHECK_STATUS(table.take(9u, held.memory, taken), Status::ok);
  CHECK_STATUS(table.name(*taken, 0x77u), Status::ok);
  CHECK(table.named(0x77u) == &held.memory);
  table.unname(0u);
  table.unname(0x78u);
  CHECK(table.named(0x77u) == &held.memory);
  table.unname(0x77u);
  CHECK(table.named(0x77u) == nullptr);
  CHECK(table.find(9u) == taken);
  CHECK(table.invariants_hold());
  table.unname(0x77u);
  CHECK_STATUS(table.name(*taken, 0x79u), Status::ok);
  CHECK(table.named(0x79u) == &held.memory);
  CHECK(table.invariants_hold()); }
