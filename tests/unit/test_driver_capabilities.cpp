#include "../../src/core/adapter_info_query.hpp"
#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/core/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "../../src/core/ddi/dxgk_wddm_device_caps_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "../../src/core/driver_capabilities.hpp"
#include "../../src/core/ddi/dxgkarg_control_mode_behavior_record.hpp"
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
constexpr std::int32_t  wddm_v2 = 0x2100;             /* DXGKDDI_WDDMv2_1 */

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

/* Constitution VI: nothing is claimed before it is implemented. WDDM 2.0 with a
 * GPU MMU, MultiEngineAware over one node and flips at device IRQL (017),
 * per-engine recovery (015), an unrestricted address range and the hardware
 * cursor (013) are the whole answer at this level. */
TEST(driver_capabilities_report_driver_caps, claims_wddm_2_1_gpu_mmu_and_what_earlier_features_implement)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  DxgkDriverCapsRecord caps;
  std::memset(&caps, 0xA5, sizeof caps);
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.WDDMVersion, wddm_v2);
  CHECK(caps.HighestAcceptableAddress == -1);
  DxgkDriverCapsRecord expected = {};
  expected.WDDMVersion = wddm_v2;
  expected.SchedulingCaps = 0x1u;          /* MultiEngineAware */
  expected.MemoryManagementCaps = 0x60u;   /* VirtualAddressingSupported | GpuMmuSupported */
  expected.GpuEngineTopologyNbAsymetricProcessingNodes = 1u;
  expected.FlipCaps = 0x2u;                /* FlipOnVSyncMmIo */
  expected.SupportPerEngineTDR = 1u;
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
  CHECK_EQ(caps.WDDMVersion, wddm_v2); }

TEST(driver_capabilities_report_wddm_device_caps, a_short_input_or_output_is_invalid)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t interface_version = 0x11007u; DxgkWddmDeviceCapsRecord caps = {};
  const DxgkargQueryAdapterInfoRecord short_input = request(&interface_version, 3u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_wddm_device_caps(AdapterInfoQuery(platform, short_input)), Status::invalid_parameter);
  const DxgkargQueryAdapterInfoRecord short_output = request(&interface_version, 4u, &caps, 3u);
  CHECK_STATUS(capabilities.report_wddm_device_caps(AdapterInfoQuery(platform, short_output)), Status::invalid_parameter);
  CHECK_EQ(caps.WDDMVersion, 0); }

TEST(driver_capabilities_answer_mode_behavior, satisfies_no_behavior)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  av::DxgkargControlModeBehaviorRecord request = {0x3u, 0xEEu, 0xEEu};
  capabilities.answer_mode_behavior(request);
  CHECK_EQ(request.Satisfied, 0u);
  CHECK_EQ(request.NotSatisfied, 0x3u);
  CHECK_EQ(request.Request, 0x3u);
  platform.set_irql(Irql::dispatch);
  capabilities.answer_mode_behavior(request);
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling)); }

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

TEST(driver_capabilities_report_physical_adapter_caps, one_gpu_mmu_node_that_also_pages)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t physical_adapter = 0u;
  av::DxgkPhysicalAdapterCapsRecord caps;
  std::memset(&caps, 0xEE, sizeof caps);
  int handle = 0;
  const DxgkargQueryAdapterInfoRecord record = request(&physical_adapter, 4u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_physical_adapter_caps(AdapterInfoQuery(platform, record), &handle), Status::ok);
  CHECK_EQ(caps.NumExecutionNodes, 1u);
  CHECK_EQ(caps.PagingNodeIndex, 0u);
  CHECK(caps.DxgkPhysicalAdapterHandle == &handle);
  CHECK_EQ(caps.Flags, 0x2u);   /* GpuMmuSupported */
  CHECK_EQ(caps.VPRPagingNode, 0u);
  CHECK_EQ(caps.VirtualCopyNodeIndex, 0u); }

TEST(driver_capabilities_report_physical_adapter_caps, another_adapter_or_short_buffers_are_invalid)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t physical_adapter = 1u;
  av::DxgkPhysicalAdapterCapsRecord caps = {};
  CHECK_STATUS(capabilities.report_physical_adapter_caps(
                   AdapterInfoQuery(platform, request(&physical_adapter, 4u, &caps, sizeof caps)), nullptr),
               Status::invalid_parameter);
  physical_adapter = 0u;
  CHECK_STATUS(capabilities.report_physical_adapter_caps(
                   AdapterInfoQuery(platform, request(&physical_adapter, 3u, &caps, sizeof caps)), nullptr),
               Status::invalid_parameter);
  CHECK_STATUS(capabilities.report_physical_adapter_caps(
                   AdapterInfoQuery(platform, request(&physical_adapter, 4u, &caps, 0x1Fu)), nullptr),
               Status::invalid_parameter);
  CHECK_EQ(caps.NumExecutionNodes, 0u);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(capabilities.report_physical_adapter_caps(
      AdapterInfoQuery(platform, request(&physical_adapter, 4u, &caps, sizeof caps)), nullptr));
  platform.set_irql(Irql::passive);
  while (platform.claim(Violation::irql_ceiling)) {} }
