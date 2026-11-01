#include "miniport/ddi/dxgk_child_container_id_record.hpp"
#include "miniport/ddi/dxgk_device_descriptor_record.hpp"
#include "miniport/ddi/dxgkarg_update_monitor_link_info_record.hpp"
#include "miniport/display/display_mode.hpp"
#include "miniport/display/edid.hpp"
#include "core/support/nt_status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>
#include <initializer_list>

using av::DisplayMode;
using av::DxgkDeviceDescriptorRecord;
using av::Edid;
using av::Irql;
using av::MockPlatform;
using av::Violation;

namespace {

constexpr std::uint32_t no_more_descriptor_data = 0xC01D0008u;
constexpr std::uint32_t invalid_parameter = 0xC000000Du;

struct Block {
    Block() : bytes{}
    {
        MockPlatform platform;
        const Edid edid(platform, DisplayMode(1280u, 800u));
        DxgkDeviceDescriptorRecord descriptor = {0u, Edid::block_bytes, bytes};
        status = static_cast<std::uint32_t>(edid.copy(descriptor).value());
    }
    std::uint32_t u16(unsigned offset) const { return static_cast<std::uint32_t>(bytes[offset] | bytes[offset + 1u] << 8u); }
    std::uint8_t  bytes[Edid::block_bytes];
    std::uint32_t status;
};

}  // namespace

TEST(edid_copy, the_whole_block_is_edid_1_3_with_a_valid_checksum)
{ const Block block;
  CHECK_EQ(block.status, 0u);
  const std::uint8_t header[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
  CHECK(std::memcmp(block.bytes, header, sizeof header) == 0);
  CHECK_EQ(block.bytes[8], 0x4Au);    /* "AV" */
  CHECK_EQ(block.bytes[9], 0xCBu);
  CHECK_EQ(block.bytes[18], 1u);
  CHECK_EQ(block.bytes[19], 3u);
  CHECK_EQ(block.bytes[20], 0x80u);   /* digital input */
  CHECK_EQ(block.bytes[21], 33u);     /* 96 dpi */
  CHECK_EQ(block.bytes[22], 21u);
  CHECK_EQ(block.bytes[24] & 0x02u, 0x02u);   /* the first detailed timing is preferred */
  CHECK_EQ(block.bytes[126], 0u);
  unsigned sum = 0u;
  for (std::uint8_t byte : block.bytes) sum += byte;
  CHECK_EQ(sum % 256u, 0u); }

TEST(edid_copy, the_preferred_timing_is_the_mode_at_60_hz)
{ const Block block;
  CHECK_EQ(block.u16(54u), 1440u * 833u * 60u / 10000u);
  CHECK_EQ(block.bytes[56] | ((block.bytes[58] >> 4u) << 8u), 1280u);
  CHECK_EQ(block.bytes[57] | ((block.bytes[58] & 0x0Fu) << 8u), 160u);
  CHECK_EQ(block.bytes[59] | ((block.bytes[61] >> 4u) << 8u), 800u);
  CHECK_EQ(block.bytes[60] | ((block.bytes[61] & 0x0Fu) << 8u), 33u);
  CHECK_EQ(block.bytes[66] | ((block.bytes[68] >> 4u) << 8u), 338u);
  CHECK_EQ(block.bytes[67] | ((block.bytes[68] & 0x0Fu) << 8u), 211u); }

/* The detailed timing states the preferred mode at the rate its 16 bit clock
 * holds: the real one where it fits, and a lower one, never a wrapped clock,
 * where it does not. */
TEST(edid_copy, a_preferred_mode_too_fast_for_the_clock_is_stated_slower_not_wrapped)
{ for (const std::uint32_t millihz : {144000u, 240000u, 360000u, 1000000u}) {
      MockPlatform platform;
      const DisplayMode mode(2560u, 1440u, millihz);
      const Edid edid(platform, mode);
      std::uint8_t bytes[Edid::block_bytes] = {};
      DxgkDeviceDescriptorRecord descriptor = {0u, Edid::block_bytes, bytes};
      CHECK_EQ(edid.copy(descriptor).value(), 0);
      const std::uint32_t clock = static_cast<std::uint32_t>(bytes[54] | bytes[55] << 8u);
      const DisplayMode   stated = mode.edid_timing();
      CHECK_EQ(clock, static_cast<std::uint32_t>(stated.pixel_rate() / 10000u));
      CHECK(clock > 0u && clock <= 65535u);
      /* Never faster than asked, and no slower than the clock forces. */
      CHECK(stated.refresh_millihz() <= millihz);
      if (millihz <= 160000u) CHECK_EQ(stated.refresh_millihz(), millihz);
      else CHECK(stated.refresh_millihz() > 150000u);
      CHECK_EQ(bytes[56] | ((bytes[58] >> 4u) << 8u), 2560u);
      CHECK_EQ(bytes[59] | ((bytes[61] >> 4u) << 8u), 1440u);
      unsigned sum = 0u;
      for (std::uint8_t byte : bytes) sum += byte;
      CHECK_EQ(sum % 256u, 0u);
  } }

TEST(edid_copy, names_the_monitor_and_its_range)
{ const Block block;
  CHECK_EQ(block.bytes[75], 0xFDu);
  CHECK_EQ(block.bytes[77], 1u);     /* vertical 1 to 255 Hz */
  CHECK_EQ(block.bytes[78], 255u);
  CHECK_EQ(block.bytes[81], 255u);   /* pixel clock to 2550 MHz */
  CHECK_EQ(block.bytes[93], 0xFCu);
  CHECK(std::memcmp(&block.bytes[95], "accel-virt\n", 11u) == 0);
  CHECK_EQ(block.bytes[111], 0x10u); }

TEST(edid_copy, a_shorter_or_later_window_copies_what_it_covers)
{ MockPlatform platform;
  const Edid edid(platform, DisplayMode(1280u, 800u));
  const Block block;
  std::uint8_t bytes[Edid::block_bytes];
  std::memset(bytes, 0xEE, sizeof bytes);
  DxgkDeviceDescriptorRecord descriptor = {0u, 10u, bytes};
  CHECK_EQ(edid.copy(descriptor).value(), 0);
  CHECK_EQ(descriptor.DescriptorLength, 10u);
  CHECK(std::memcmp(bytes, block.bytes, 10u) == 0);
  CHECK_EQ(bytes[10], 0xEEu);
  descriptor = {120u, Edid::block_bytes, bytes};
  CHECK_EQ(edid.copy(descriptor).value(), 0);
  CHECK_EQ(descriptor.DescriptorLength, 8u);
  CHECK(std::memcmp(bytes, &block.bytes[120], 8u) == 0); }

TEST(edid_copy, at_or_past_the_end_there_is_no_more_data)
{ MockPlatform platform;
  const Edid edid(platform, DisplayMode(1280u, 800u));
  std::uint8_t bytes[1] = {0xEE};
  DxgkDeviceDescriptorRecord descriptor = {Edid::block_bytes, 1u, bytes};
  CHECK_EQ(static_cast<std::uint32_t>(edid.copy(descriptor).value()), no_more_descriptor_data);
  descriptor.DescriptorOffset = 0xFFFFFFFFu;
  CHECK_EQ(static_cast<std::uint32_t>(edid.copy(descriptor).value()), no_more_descriptor_data);
  CHECK_EQ(bytes[0], 0xEEu); }

TEST(edid_copy, a_null_buffer_is_invalid)
{ MockPlatform platform;
  const Edid edid(platform, DisplayMode(1280u, 800u));
  DxgkDeviceDescriptorRecord descriptor = {0u, 1u, nullptr};
  CHECK_EQ(static_cast<std::uint32_t>(edid.copy(descriptor).value()), invalid_parameter); }

TEST(edid_describe, fills_the_eld_from_the_block)
{ MockPlatform platform;
  const Edid edid(platform, DisplayMode(1280u, 800u));
  av::DxgkChildContainerIdRecord container = {};
  std::memset(&container, 0xEE, sizeof container);
  edid.describe(container);
  CHECK_EQ(container.EldInfoPortId, 0u);
  CHECK_EQ(container.EldInfoManufacturerName, 0xCB4Au);
  CHECK_EQ(container.EldInfoProductCode, 1u);
  CHECK_EQ(container.ContainerId[0], 0xEEu);
  platform.set_irql(Irql::dispatch);
  edid.describe(container);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

TEST(edid_describe_link, a_mono_monitor_without_colour_extensions)
{ MockPlatform platform;
  const Edid edid(platform, DisplayMode(1280u, 800u));
  av::DxgkargUpdateMonitorLinkInfoRecord link = {0u, 0x3u, 0xEEu, 0xEEu};
  edid.describe_link(link);
  CHECK_EQ(link.MonitorLinkInfoUsageHints, 0x3u);
  CHECK_EQ(link.MonitorLinkInfoCapabilities, 0u);
  CHECK_EQ(link.MonitorLinkInfoDitheringSupport, 0u);
  platform.set_irql(Irql::dispatch);
  edid.describe_link(link);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

TEST(edid_copy, is_passive_only)
{ MockPlatform platform;
  const Edid edid(platform, DisplayMode(1280u, 800u));
  std::uint8_t bytes[1];
  DxgkDeviceDescriptorRecord descriptor = {0u, 1u, bytes};
  platform.set_irql(Irql::dispatch);
  static_cast<void>(edid.copy(descriptor));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }
