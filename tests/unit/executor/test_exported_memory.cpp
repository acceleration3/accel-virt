#include "executor/exported_memory.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ExportedMemory;

namespace {

std::uint8_t mapping[64];

}

TEST(exported_memory, nothing_is_not_made_and_names_no_descriptor)
{ const ExportedMemory nothing;
  CHECK(!nothing.made());
  CHECK_EQ(nothing.handle(), 0u);
  CHECK_EQ(nothing.fd(), -1);
  CHECK(nothing.pointer() == nullptr);
  CHECK_EQ(nothing.size(), 0u); }

TEST(exported_memory, an_allocation_answers_every_field_it_was_made_with)
{ const ExportedMemory memory(0xDEAD0000BEEFull, 17, mapping, 0x10000u);
  CHECK(memory.made());
  CHECK_EQ(memory.handle(), 0xDEAD0000BEEFull);
  CHECK_EQ(memory.fd(), 17);
  CHECK(memory.pointer() == mapping);
  CHECK_EQ(memory.size(), 0x10000u); }

TEST(exported_memory, made_is_decided_by_the_handle_alone)
{ CHECK(ExportedMemory(1u, -1, nullptr, 0u).made());
  CHECK(!ExportedMemory(0u, 5, mapping, 4096u).made()); }

TEST(exported_memory, a_copy_names_the_same_allocation)
{ const ExportedMemory memory(9u, 3, mapping, 4096u);
  ExportedMemory       copy;
  copy = memory;
  CHECK_EQ(copy.handle(), 9u);
  CHECK_EQ(copy.fd(), 3);
  CHECK(copy.pointer() == mapping);
  CHECK_EQ(copy.size(), 4096u); }
