#include "../../src/core/adapter_info_query.hpp"
#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/core/ddi/dxgk_wddm_device_caps_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "../../src/core/driver_capabilities.hpp"
#include "../harness/guarded_buffer.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using av::AdapterInfoQuery;
using av::DriverCapabilities;
using av::DxgkDriverCapsRecord;
using av::DxgkWddmDeviceCapsRecord;
using av::DxgkargQueryAdapterInfoRecord;
using av::GuardedBuffer;
using av::Irql;
using av::MockPlatform;
using av::Status;
using av::Violation;

namespace {

constexpr std::uint32_t driver_caps_bytes = 0x250u;   /* GetDriverCapsSizeFromDdiVersion(0x11007) */
constexpr std::int32_t  wddm_v1 = 0x1000;             /* DXGKDDI_WDDMv1 */

DxgkargQueryAdapterInfoRecord request(void *input, std::uint32_t input_size, void *output, std::uint32_t output_size)
{
    DxgkargQueryAdapterInfoRecord record = {};
    record.pInputData = input;
    record.InputDataSize = input_size;
    record.pOutputData = output;
    record.OutputDataSize = output_size;
    return record;
}

}  // namespace

TEST(driver_capabilities_report_driver_caps, fills_exactly_the_wddm_3_2_block)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  GuardedBuffer output(driver_caps_bytes);
  std::memset(output.data(), 0xA5, driver_caps_bytes);
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, output.data(), driver_caps_bytes);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK(output.guards_intact()); }

/* Constitution VI: nothing is claimed before it is implemented. WDDM 1.0, an
 * unrestricted address range and the hardware cursor (feature 013) are the
 * whole answer at this level. */
TEST(driver_capabilities_report_driver_caps, claims_wddm_1_0_and_the_hardware_cursor)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  DxgkDriverCapsRecord caps;
  std::memset(&caps, 0xA5, sizeof caps);
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.WDDMVersion, wddm_v1);
  CHECK(caps.HighestAcceptableAddress == -1);
  DxgkDriverCapsRecord expected = {};
  expected.WDDMVersion = wddm_v1;
  expected.HighestAcceptableAddress = -1;
  expected.MaxPointerWidth = 64u;
  expected.MaxPointerHeight = 64u;
  expected.PointerCaps = 0x7u;   /* Monochrome | Color | MaskedColor */
  CHECK(std::memcmp(&caps, &expected, sizeof caps) == 0); }

TEST(driver_capabilities_report_driver_caps, a_short_output_is_invalid)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint8_t output[driver_caps_bytes] = {};
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, output, driver_caps_bytes - 1u);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::invalid_parameter); }

TEST(driver_capabilities_report_wddm_device_caps, answers_the_same_wddm_version)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t interface_version = 0x11007u; DxgkWddmDeviceCapsRecord caps = {};
  const DxgkargQueryAdapterInfoRecord record = request(&interface_version, 4u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_wddm_device_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.WDDMVersion, wddm_v1); }

TEST(driver_capabilities_report_wddm_device_caps, a_short_input_or_output_is_invalid)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t interface_version = 0x11007u; DxgkWddmDeviceCapsRecord caps = {};
  const DxgkargQueryAdapterInfoRecord short_input = request(&interface_version, 3u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_wddm_device_caps(AdapterInfoQuery(platform, short_input)), Status::invalid_parameter);
  const DxgkargQueryAdapterInfoRecord short_output = request(&interface_version, 4u, &caps, 3u);
  CHECK_STATUS(capabilities.report_wddm_device_caps(AdapterInfoQuery(platform, short_output)), Status::invalid_parameter);
  CHECK_EQ(caps.WDDMVersion, 0); }

TEST(driver_capabilities, is_passive_only)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  DxgkDriverCapsRecord caps = {}; std::uint32_t interface_version = 0u;
  const DxgkargQueryAdapterInfoRecord driver = request(nullptr, 0u, &caps, sizeof caps);
  const DxgkargQueryAdapterInfoRecord device = request(&interface_version, 4u, &caps, sizeof caps);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(capabilities.report_driver_caps(AdapterInfoQuery(platform, driver)));
  static_cast<void>(capabilities.report_wddm_device_caps(AdapterInfoQuery(platform, device)));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }
