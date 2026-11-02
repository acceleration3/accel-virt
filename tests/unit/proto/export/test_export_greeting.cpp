#include "harness/framework/test_registry.hpp"
#include "proto/export/export_greeting.hpp"

#include <cstdint>
#include <cstring>

using av::ExportGreeting;

namespace {

const std::uint8_t uuid[ExportGreeting::uuid_bytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

}

TEST(export_greeting, a_gpu_on_pci_keeps_its_address)
{ const ExportGreeting greeting(uuid, 0x10000u, 0x03u, 0x1Fu, 0x07u);
  CHECK(std::memcmp(greeting.uuid(), uuid, sizeof uuid) == 0);
  CHECK(greeting.on_pci());
  CHECK_EQ(greeting.domain(), 0x10000u);
  CHECK_EQ(greeting.bus(), 0x03u);
  CHECK_EQ(greeting.device(), 0x1Fu);
  CHECK_EQ(greeting.function(), 0x07u);
  CHECK(greeting.valid());
  CHECK(!greeting.empty()); }

TEST(export_greeting, a_gpu_off_pci_has_no_address)
{ const ExportGreeting greeting(uuid);
  CHECK(!greeting.on_pci());
  CHECK_EQ(greeting.domain(), 0u);
  CHECK_EQ(greeting.bus(), 0u);
  CHECK(greeting.valid());
  CHECK(!greeting.empty()); }

TEST(export_greeting, valid_needs_a_uuid_and_empty_needs_nothing_set)
{ const std::uint8_t zero[ExportGreeting::uuid_bytes] = {};
  std::uint8_t       last[ExportGreeting::uuid_bytes] = {};
  last[ExportGreeting::uuid_bytes - 1u] = 1u;
  CHECK(ExportGreeting(last).valid());
  CHECK(!ExportGreeting(last).empty());
  CHECK(!ExportGreeting(zero).valid());
  CHECK(ExportGreeting(zero).empty());
  CHECK(!ExportGreeting(zero, 0u, 0u, 0u, 0u).empty());
  CHECK(!ExportGreeting(zero, 0u, 0u, 0u, 0u).valid());
  CHECK(ExportGreeting().empty());
  CHECK(!ExportGreeting().valid()); }
