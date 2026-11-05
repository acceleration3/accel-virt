#include "executor/exported_memory.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "harness/executor/imported_blob_probe.hpp"
#include "harness/framework/test_registry.hpp"

#include <stdint.h>

using av::ExportedMemory;
using av::ImportedBlob;
using av::ImportedBlobProbe;
using av::SourceMemory;
using av::Status;

namespace {

constexpr uint64_t page = 4096u;

uint8_t backing[page];

class Memory {
public:
    Memory() : memory(1u, 0x10u, 2u, 1u, ExportedMemory(0x77u, -1, backing, page)) {}

    SourceMemory memory;
};

}

TEST(imported_blob, an_empty_slot_holds_nothing)
{ ImportedBlob blob;
  CHECK(!blob.live());
  CHECK(!blob.holds(0u));
  CHECK(!blob.holds(1u));
  CHECK_EQ(blob.id(), 0u);
  CHECK_EQ(blob.handle(), 0u);
  CHECK(!blob.arena());
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, takes_memory_under_the_id_and_answers_it)
{ Memory       held;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(7u, held.memory), Status::ok);
  CHECK(blob.live());
  CHECK(blob.holds(7u));
  CHECK(!blob.holds(8u));
  CHECK_EQ(blob.id(), 7u);
  CHECK(&blob.memory() == &held.memory);
  CHECK(blob.memory().pointer() == backing);
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, an_id_of_zero_is_held_like_any_other)
{ Memory       held;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(0u, held.memory), Status::ok);
  CHECK(blob.live());
  CHECK(blob.holds(0u));
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, a_slot_already_holding_one_is_not_overwritten)
{ Memory       first;
  Memory       second;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, first.memory), Status::ok);
  ImportedBlobProbe(blob).record(0x20u);
  CHECK_STATUS(blob.take(2u, second.memory), Status::invalid_state);
  CHECK(blob.holds(1u));
  CHECK(!blob.holds(2u));
  CHECK(&blob.memory() == &first.memory);
  CHECK_EQ(blob.handle(), 0x20u);
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, giving_it_back_answers_the_memory_once_and_leaves_nothing_behind)
{ Memory       held;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(3u, held.memory), Status::ok);
  ImportedBlobProbe(blob).record(0x5678u);
  blob.keep_as_arena();
  CHECK(blob.give_back() == &held.memory);
  CHECK(!blob.live());
  CHECK(!blob.holds(3u));
  CHECK(!blob.arena());
  CHECK_EQ(blob.id(), 0u);
  CHECK_EQ(blob.handle(), 0u);
  CHECK(blob.invariants_hold());
  CHECK(blob.give_back() == nullptr);
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, a_slot_taken_again_starts_with_nothing_recorded)
{ Memory       first;
  Memory       second;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, first.memory), Status::ok);
  ImportedBlobProbe(blob).record(0x20u);
  blob.keep_as_arena();
  static_cast<void>(blob.give_back());
  CHECK_STATUS(blob.take(2u, second.memory), Status::ok);
  CHECK_EQ(blob.handle(), 0u);
  CHECK(!blob.arena());
  CHECK(&blob.memory() == &second.memory);
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, remembers_the_wire_handle_the_guest_names)
{ Memory       held;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, held.memory), Status::ok);
  ImportedBlobProbe(blob).record(0xFEEDu);
  CHECK_EQ(blob.handle(), 0xFEEDu);
  CHECK(blob.invariants_hold());
  ImportedBlobProbe(blob).record(0u);
  CHECK_EQ(blob.handle(), 0u);
  CHECK(blob.invariants_hold()); }

TEST(imported_blob, an_arena_is_kept_only_while_a_memory_is_held)
{ Memory       held;
  ImportedBlob blob;
  blob.keep_as_arena();
  CHECK(!blob.arena());
  CHECK_STATUS(blob.take(1u, held.memory), Status::ok);
  CHECK(!blob.arena());
  blob.keep_as_arena();
  CHECK(blob.arena());
  CHECK(blob.invariants_hold());
  static_cast<void>(blob.give_back()); }

TEST(imported_blob_invariants, an_empty_slot_with_an_id_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_id(4u);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_with_a_handle_breaks_them)
{ ImportedBlob blob;
  ImportedBlobProbe(blob).set_handle(0x1000u);
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, an_empty_slot_that_is_an_arena_breaks_them)
{ Memory       held;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(1u, held.memory), Status::ok);
  blob.keep_as_arena();
  ImportedBlobProbe(blob).forget_memory();
  CHECK(!blob.invariants_hold()); }

TEST(imported_blob_invariants, a_live_slot_holds_them_whatever_it_records)
{ Memory       held;
  ImportedBlob blob;
  CHECK_STATUS(blob.take(9u, held.memory), Status::ok);
  ImportedBlobProbe(blob).record(0x99u);
  CHECK(blob.invariants_hold());
  static_cast<void>(blob.give_back()); }
