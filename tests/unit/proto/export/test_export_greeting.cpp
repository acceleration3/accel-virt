/* What hello answers: the GPU's UUID and, where it has one, its PCI address,
 * and how many surfaces a connection holds. Valid with a capacity, and with no
 * address fields set when it is not on PCI; empty only when nothing is set. */
#include "harness/framework/test_registry.hpp"
#include "proto/export/export_greeting.hpp"

#include <cstdint>
#include <cstring>

using av::ExportGreeting;

namespace {

const std::uint8_t uuid[ExportGreeting::uuid_bytes] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

}  // namespace

TEST(export_greeting, a_gpu_on_pci_keeps_its_address)
{ const ExportGreeting greeting(uuid, 64u, 0x10000u, 0x03u, 0x1Fu, 0x07u);
  CHECK(std::memcmp(greeting.uuid(), uuid, sizeof uuid) == 0);
  CHECK_EQ(greeting.capacity(), 64u);
  CHECK(greeting.on_pci());
  CHECK_EQ(greeting.domain(), 0x10000u);
  CHECK_EQ(greeting.bus(), 0x03u);
  CHECK_EQ(greeting.device(), 0x1Fu);
  CHECK_EQ(greeting.function(), 0x07u);
  CHECK(greeting.valid());
  CHECK(!greeting.empty()); }

TEST(export_greeting, a_gpu_off_pci_has_no_address)
{ const ExportGreeting greeting(uuid, 1u);
  CHECK(!greeting.on_pci());
  CHECK_EQ(greeting.domain(), 0u);
  CHECK_EQ(greeting.bus(), 0u);
  CHECK(greeting.valid());
  CHECK(!greeting.empty()); }

TEST(export_greeting, valid_needs_a_capacity_and_empty_needs_nothing_set)
{ CHECK(!ExportGreeting(uuid, 0u).valid());
  CHECK(!ExportGreeting(uuid, 0u).empty());   /* the uuid is set */
  const std::uint8_t zero[ExportGreeting::uuid_bytes] = {};
  CHECK(ExportGreeting(zero, 0u).empty());
  CHECK(!ExportGreeting(zero, 0u, 0u, 0u, 0u, 0u).empty());   /* on PCI at 0:0:0.0 */
  CHECK(!ExportGreeting(zero, 0u, 0u, 0u, 0u, 0u).valid());
  CHECK(ExportGreeting().empty());
  CHECK(!ExportGreeting().valid()); }
