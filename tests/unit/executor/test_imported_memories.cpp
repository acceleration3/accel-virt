/* The memory a session imported besides its region, each named on the wire by
 * the memory's own address. What a caller acts on: the handle an adopted
 * import is answered with, which memory a handle a command names finds, and
 * that a handle given back finds nothing. */
#include "device/abi/av_executor.h"
#include "executor/exported_memory.hpp"
#include "executor/imported_blob.hpp"
#include "executor/imported_memories.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/imported_memories_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExportedMemory;
using av::ImportedBlob;
using av::ImportedMemories;
using av::ImportedMemoriesProbe;
using av::SourceMemory;
using av::Status;

namespace {

std::uint8_t backing[4096];

class Memory {
public:
    explicit Memory(std::uint64_t key)
        : memory(1u, key, 0u, AV_MEMORY_WRITE_BACK, ExportedMemory(key, -1, backing, sizeof backing))
    {}

    SourceMemory memory;
};

std::uint64_t address_of(const SourceMemory &memory)
{
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&memory));
}

}  // namespace

TEST(imported_memories, an_adopted_import_is_named_by_its_memory_s_address)
{ ImportedMemories memories;
  Memory           heap(1u);
  ImportedBlob     blob;
  CHECK_STATUS(blob.take(5u, heap.memory), Status::ok);
  CHECK(memories.adopt(blob));
  CHECK_EQ(blob.object(), address_of(heap.memory));
  CHECK_EQ(blob.wire_handle(), address_of(heap.memory));
  CHECK_EQ(memories.count(), 1u);
  CHECK(memories.find(blob.wire_handle()) == &heap.memory);
  CHECK(memories.invariants_hold()); }

/* A handle no live import has - none at all, one never adopted, one given
 * back - finds nothing, so a command naming it is refused. */
TEST(imported_memories, a_handle_no_live_import_has_finds_nothing)
{ ImportedMemories memories;
  Memory           heap(1u);
  Memory           other(2u);
  CHECK(memories.find(0u) == nullptr);
  CHECK(memories.find(address_of(heap.memory)) == nullptr);
  ImportedBlob blob;
  CHECK_STATUS(blob.take(5u, heap.memory), Status::ok);
  CHECK(memories.adopt(blob));
  const std::uint64_t handle = blob.wire_handle();
  CHECK(memories.find(address_of(other.memory)) == nullptr);
  CHECK(memories.find(handle + 1u) == nullptr);
  memories.abandon(blob);
  CHECK_EQ(blob.object(), 0u);
  CHECK_EQ(blob.wire_handle(), 0u);
  CHECK_EQ(memories.count(), 0u);
  CHECK(memories.find(handle) == nullptr);
  CHECK(memories.invariants_hold()); }

/* Giving back an import this never adopted leaves the others, and their
 * handles, as they were. */
TEST(imported_memories, abandoning_an_import_it_never_adopted_changes_nothing)
{ ImportedMemories memories;
  Memory           heap(1u);
  Memory           stranger(2u);
  ImportedBlob     adopted;
  ImportedBlob     never;
  CHECK_STATUS(adopted.take(5u, heap.memory), Status::ok);
  CHECK_STATUS(never.take(6u, stranger.memory), Status::ok);
  CHECK(memories.adopt(adopted));
  never.record(0x77u, 0x77u);
  memories.abandon(never);
  CHECK_EQ(never.object(), 0x77u);
  CHECK_EQ(memories.count(), 1u);
  CHECK(memories.find(adopted.wire_handle()) == &heap.memory);
  CHECK(memories.invariants_hold()); }

/* As many as a session's import table holds, and not one more: a full table
 * refuses and records nothing; a slot given back takes the next. */
TEST(imported_memories, a_full_table_refuses_and_records_nothing)
{ ImportedMemories memories;
  static Memory     heap(1u);
  static ImportedBlob blobs[ImportedMemories::capacity + 1u];
  for (std::uint32_t index = 0u; index < ImportedMemories::capacity; index++) {
      CHECK_STATUS(blobs[index].take(index + 1u, heap.memory), Status::ok);
      CHECK(memories.adopt(blobs[index]));
  }
  ImportedBlob &extra = blobs[ImportedMemories::capacity];
  CHECK_STATUS(extra.take(0xFFFFu, heap.memory), Status::ok);
  CHECK(!memories.adopt(extra));
  CHECK_EQ(extra.object(), 0u);
  CHECK_EQ(memories.count(), ImportedMemories::capacity);
  memories.abandon(blobs[0]);
  CHECK(memories.adopt(extra));
  CHECK_EQ(memories.count(), ImportedMemories::capacity);
  CHECK(memories.invariants_hold());
  memories.forget_all();
  for (ImportedBlob &blob : blobs) static_cast<void>(blob.give_back()); }

/* A session that ends forgets every import at once. */
TEST(imported_memories, forgetting_all_leaves_no_handle_to_find)
{ ImportedMemories memories;
  Memory           first(1u);
  Memory           second(2u);
  ImportedBlob     a;
  ImportedBlob     b;
  CHECK_STATUS(a.take(1u, first.memory), Status::ok);
  CHECK_STATUS(b.take(2u, second.memory), Status::ok);
  CHECK(memories.adopt(a));
  CHECK(memories.adopt(b));
  memories.forget_all();
  CHECK_EQ(memories.count(), 0u);
  CHECK(memories.find(a.wire_handle()) == nullptr);
  CHECK(memories.find(b.wire_handle()) == nullptr);
  CHECK(memories.invariants_hold()); }

/* A count that disagrees with the slots is a table that would refuse while it
 * has room, or stop seeing what it holds. */
TEST(imported_memories_invariants, a_count_that_disagrees_with_the_slots_breaks_them)
{ ImportedMemories memories;
  ImportedMemoriesProbe(memories).set_count(1u);
  CHECK(!memories.invariants_hold()); }
