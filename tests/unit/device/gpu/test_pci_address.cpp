#include "core/support/text_buffer.hpp"
#include "device/gpu/pci_address.hpp"
#include "harness/framework/test_registry.hpp"
#include "proto/export/export_greeting.hpp"

#include <cstdint>
#include <cstring>

using av::PciAddress;
using av::TextBuffer;

namespace {

bool spelled(const PciAddress &address, const char *expected)
{
    char       bytes[32] = {};
    TextBuffer text(bytes, sizeof bytes);
    address.append_to(text);
    return std::strcmp(text.text(), expected) == 0;
}

}  // namespace

TEST(pci_address, is_spelled_the_way_sysfs_names_the_function)
{ CHECK(spelled(PciAddress(0u, 0x03u, 0u, 0u), "0000:03:00.0"));
  CHECK(spelled(PciAddress(0u, 0x7Bu, 0u, 0u), "0000:7b:00.0"));
  CHECK(spelled(PciAddress(0x10u, 0xC1u, 0x1Fu, 7u), "0010:c1:1f.7"));
  CHECK(spelled(PciAddress(), "0000:00:00.0")); }

/* A driver's out-of-range field cannot make the path into another shape. */
TEST(pci_address, cuts_each_field_to_the_width_pci_gives_it)
{ CHECK(spelled(PciAddress(0x12345u, 0x1FFu, 0x3Fu, 0xFu), "2345:ff:1f.7")); }

/* What the export socket's hello names the GPU by: the address, cut as sysfs
 * cuts it, and the UUID and capacity it was given. */
TEST(pci_address, greets_an_export_consumer_with_the_address_it_holds)
{ const std::uint8_t uuid[av::ExportGreeting::uuid_bytes] = {9u, 8u, 7u};
  const av::ExportGreeting greeting = av::PciAddress(0x10002u, 0x103u, 0x21u, 0x9u).greeting(uuid);
  CHECK(greeting.on_pci());
  CHECK_EQ(greeting.domain(), 0x0002u);
  CHECK_EQ(greeting.bus(), 0x03u);
  CHECK_EQ(greeting.device(), 0x01u);
  CHECK_EQ(greeting.function(), 0x1u);
  CHECK_EQ(greeting.uuid()[2], 7u);
  CHECK(greeting.valid()); }

