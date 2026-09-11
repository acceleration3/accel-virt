#include "core/pci/mapped_register_window.hpp"
#include "harness/device/fake_register_bytes.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::FakeRegisterBytes;
using av::Irql;
using av::MappedRegisterWindow;
using av::MockPlatform;
using av::Violation;

namespace {

const auto bar = reinterpret_cast<std::uint8_t *>(std::uintptr_t{0x7F0000100000u});

}  // namespace

TEST(mapped_register_window, reads_and_writes_at_its_base_plus_the_offset)
{ MockPlatform platform; FakeRegisterBytes device(32u);
  platform.attach_register_window(bar + 0x2000u, device);
  MappedRegisterWindow window(platform, bar + 0x2000u, 32u);
  CHECK(window.length() == 32u);
  window.write_u8(1u, 0x5Au);
  CHECK_EQ(device.last_write_offset(), 1u);
  window.write_u16(2u, 0xBEEFu);
  window.write_u32(4u, 0x01020304u);
  window.write_u64(8u, 0x0A0B0C0D0E0F1011ull);
  CHECK_EQ(device.last_write_offset(), 8u);
  CHECK_EQ(window.read_u8(1u), 0x5Au);
  CHECK_EQ(window.read_u16(2u), 0xBEEFu);
  CHECK_EQ(window.read_u32(4u), 0x01020304u);
  CHECK_EQ(window.read_u32(12u), 0x0A0B0C0Du); }

TEST(mapped_register_window, an_access_past_its_mapping_reaches_no_device)
{ MockPlatform platform; FakeRegisterBytes device(4u);
  platform.attach_register_window(bar, device);
  MappedRegisterWindow window(platform, bar, 4u);
  CHECK_EQ(window.read_u8(4u), 0xFFu);
  CHECK(platform.claim(Violation::unmapped_register)); }

TEST(mapped_register_window, is_usable_from_an_interrupt)
{ MockPlatform platform; FakeRegisterBytes device(4u);
  platform.attach_register_window(bar, device);
  MappedRegisterWindow window(platform, bar, 4u);
  platform.set_irql(Irql::isr);
  CHECK_EQ(window.read_u8(0u), 0u);
  CHECK_EQ(platform.unclaimed_violations(), 0u); }
