#include "core/support/text_buffer.hpp"
#include "device/gpu/pci_address.hpp"
#include "harness/framework/test_registry.hpp"

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
