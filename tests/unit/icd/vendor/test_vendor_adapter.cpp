/* One avgpu adapter as the vendor libraries describe it: its name, taken from
 * the UTF-16 Windows keeps it in, and its driver package's version in the
 * form Windows writes it. */
#include "core/support/text_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/vendor/vendor_adapter.hpp"

#include <cstdint>
#include <cstring>

using av::PciIdentity;
using av::PciLocation;
using av::TextBuffer;
using av::vendor::VendorAdapter;

TEST(vendor_adapter, carries_every_fact_it_was_made_with)
{ const VendorAdapter adapter(0x0000000500001234ull, PciIdentity(0x1002u, 0x13C0u, 0x1AF4u, 0x5256u, 0xC9u),
                              PciLocation(0u, 17u, 0u), 0x0000002300050000ull, 512ull << 20u, 0u, 4ull << 30u,
                              true, true);
  CHECK_EQ(adapter.luid(), 0x0000000500001234ull);
  CHECK_EQ(adapter.identity().vendor(), 0x1002u);
  CHECK_EQ(adapter.identity().revision(), 0xC9u);
  CHECK_EQ(adapter.location().device(), 17u);
  CHECK_EQ(adapter.driver_version(), 0x0000002300050000ull);
  CHECK_EQ(adapter.dedicated_video_bytes(), 512ull << 20u);
  CHECK_EQ(adapter.dedicated_system_bytes(), 0u);
  CHECK_EQ(adapter.shared_system_bytes(), 4ull << 30u);
  CHECK(adapter.integrated());
  CHECK(adapter.primary());
  CHECK_EQ(adapter.name()[0], '\0');
  const VendorAdapter empty;
  CHECK_EQ(empty.luid(), 0u);
  CHECK(!empty.integrated());
  CHECK(!empty.primary()); }

TEST(vendor_adapter, writes_the_driver_version_as_windows_does)
{ const VendorAdapter adapter(1u, PciIdentity(), PciLocation(), 0x0000002300050000ull, 0u, 0u, 0u, false, false);
  char       bytes[32] = {};
  TextBuffer text(bytes, sizeof bytes);
  adapter.append_driver_version(text);
  CHECK(std::strcmp(bytes, "0.35.5.0") == 0);
  const VendorAdapter wide(1u, PciIdentity(), PciLocation(), 0x0001FFFF00020003ull, 0u, 0u, 0u, false, false);
  char       more[32] = {};
  TextBuffer again(more, sizeof more);
  wide.append_driver_version(again);
  CHECK(std::strcmp(more, "1.65535.2.3") == 0); }

/* The name is the adapter's own, as icd::AdapterString takes it. */
TEST(vendor_adapter, takes_its_name_from_utf16)
{ VendorAdapter adapter;
  const std::uint16_t ascii[] = {'A', 'M', 'D', ' ', 'R', 'a', 'd', 'e', 'o', 'n', 0u};
  adapter.take_name(ascii);
  CHECK(std::strcmp(adapter.name(), "AMD Radeon") == 0); }
