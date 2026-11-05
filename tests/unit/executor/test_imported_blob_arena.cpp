#include "executor/exported_memory.hpp"
#include "executor/imported_blob.hpp"
#include "executor/source_memory.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExportedMemory;
using av::ImportedBlob;
using av::SourceMemory;
using av::Status;

namespace {

alignas(4096) std::uint8_t mapping[4096];

}

TEST(imported_blob_arena, a_blob_is_plain_until_it_is_kept_as_an_arena_segment)
{ SourceMemory memory(1u, 0x40u, 0u, 1u, ExportedMemory(0x99u, -1, mapping, sizeof mapping));
  ImportedBlob blob;
  CHECK(!blob.arena());
  CHECK_STATUS(blob.take(5u, memory), Status::ok);
  CHECK(!blob.arena());
  blob.keep_as_arena();
  CHECK(blob.arena());
  CHECK(blob.invariants_hold()); }

TEST(imported_blob_arena, an_empty_blob_cannot_be_an_arena_segment)
{ ImportedBlob blob;
  blob.keep_as_arena();
  CHECK(!blob.arena());
  CHECK(blob.invariants_hold()); }

TEST(imported_blob_arena, giving_the_memory_back_forgets_the_role_and_a_new_take_starts_plain)
{ SourceMemory memory(1u, 0x40u, 0u, 1u, ExportedMemory(0x99u, -1, mapping, sizeof mapping));
  ImportedBlob blob;
  CHECK_STATUS(blob.take(5u, memory), Status::ok);
  blob.keep_as_arena();
  CHECK(blob.give_back() == &memory);
  CHECK(!blob.arena());
  CHECK(blob.invariants_hold());
  CHECK_STATUS(blob.take(6u, memory), Status::ok);
  CHECK(!blob.arena()); }
