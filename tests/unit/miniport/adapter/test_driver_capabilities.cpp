#include "miniport/adapter/driver_capabilities.hpp"
#include "miniport/ddi/dxgkarg_native_fence_records.hpp"
#include "miniport/ddi/dxgk_driver_caps_record.hpp"
#include "miniport/ddi/dxgk_physical_adapter_caps_record.hpp"
#include "miniport/ddi/dxgk_wddm_device_caps_record.hpp"
#include "miniport/ddi/dxgkarg_control_mode_behavior_record.hpp"
#include "miniport/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "miniport/driver/adapter_info_query.hpp"
#include "harness/framework/guarded_buffer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

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
constexpr std::int32_t  wddm_version = 0x3200;             /* DXGKDDI_WDDMv3_2 */

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

/* Constitution VI: nothing is claimed before it is implemented. WDDM 3.2 with a
 * GPU MMU, MultiEngineAware over one node and flips at device IRQL (017),
 * per-engine recovery (015), an unrestricted address range and the hardware
 * cursor (013) are the whole answer at this level. */
TEST(driver_capabilities_report_driver_caps, claims_wddm_3_2_gpu_mmu_and_what_earlier_features_implement)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  DxgkDriverCapsRecord caps;
  std::memset(&caps, 0xA5, sizeof caps);
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.WDDMVersion, wddm_version);
  CHECK(caps.HighestAcceptableAddress == -1);
  DxgkDriverCapsRecord expected = {};
  expected.WDDMVersion = wddm_version;
  /* MultiEngineAware alone until the OS allows native fences: claiming
   * NativeGpuFence while the feature is disabled fails the adapter start
   * (feature 049, research.md §17). */
  expected.SchedulingCaps = 0x1u;
  expected.MemoryManagementCaps = 0x70u;   /* CrossAdapterResource | VirtualAddressingSupported | GpuMmuSupported */
  expected.GpuEngineTopologyNbAsymetricProcessingNodes = 2u;
  expected.FlipCaps = 0x2u;                /* FlipOnVSyncMmIo */
  expected.PresentationCaps = 0x6c804u;        /* SupportKernelModeCommandBuffer, and not SupportSoftwareDeviceBitmaps */
  expected.SupportPerEngineTDR = 1u;
  expected.SupportDirectFlip = 1u;
  expected.SupportMultiPlaneOverlay = 1u;
  expected.MaxOverlayPlanes = 4u;
  expected.HighestAcceptableAddress = -1;
  expected.MaxPointerWidth = 64u;
  expected.MaxPointerHeight = 64u;
  expected.PointerCaps = 0x7u;   /* Monochrome | Color | MaskedColor */
  CHECK(std::memcmp(&caps, &expected, sizeof caps) == 0); }

/* The bit is a promise of DxgkDdiRenderKm, which dxgkrnl rejects the adapter
 * for breaking (docs/ddi-sequences.md, 2.3). */
TEST(driver_capabilities_report_driver_caps, reports_the_kernel_mode_command_buffer_bit_and_no_software_device_bitmaps)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  DxgkDriverCapsRecord caps = {};
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.PresentationCaps & 0x4u, 0x4u);
  CHECK_EQ(caps.PresentationCaps & 0x10000000u, 0u); }

/* FR-006 named out loud: rotation and gamma are what the display path would
 * otherwise silently ignore, so each field that would claim one is asserted
 * zero by name. Overlay planes it does have - four, beside DirectFlip, which
 * dxgkrnl insists on (OverlaySupport) - but none flipped immediately, no
 * scaled cursor and no queue of overlay flips. */
TEST(driver_capabilities_report_driver_caps, claims_no_rotation_or_gamma_and_four_overlay_planes)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  DxgkDriverCapsRecord caps;
  std::memset(&caps, 0xA5, sizeof caps);
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.SupportSmoothRotation, 0u);
  CHECK_EQ(caps.GammaRampCaps, 0u);
  CHECK_EQ(caps.SupportMultiPlaneOverlay, 1u);
  CHECK_EQ(caps.SupportDirectFlip, 1u);
  CHECK_EQ(caps.SupportMultiPlaneOverlayImmediateFlip, 0u);
  CHECK_EQ(caps.CursorScaledWithMultiPlaneOverlayPlane0, 0u);
  CHECK_EQ(caps.MaxOverlays, 0u);
  CHECK_EQ(caps.MaxOverlayPlanes, 4u);
  CHECK_EQ(caps.MaxQueuedMultiPlaneOverlayFlipVSync, 0u); }

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
  CHECK_EQ(caps.WDDMVersion, wddm_version); }

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

TEST(driver_capabilities_report_display_caps_extension, claims_no_display_extension)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t extension = 0xFFFFFFFFu;
  CHECK_STATUS(capabilities.report_display_caps_extension(
                   AdapterInfoQuery(platform, request(nullptr, 0u, &extension, sizeof extension))), Status::ok);
  CHECK_EQ(extension, 0u);
  CHECK_STATUS(capabilities.report_display_caps_extension(
                   AdapterInfoQuery(platform, request(nullptr, 0u, &extension, 3u))), Status::invalid_parameter);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(capabilities.report_display_caps_extension(
      AdapterInfoQuery(platform, request(nullptr, 0u, &extension, sizeof extension))));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }

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

TEST(driver_capabilities_report_physical_adapter_caps, two_gpu_mmu_nodes_the_first_of_which_pages)
{ MockPlatform platform; const DriverCapabilities capabilities(platform);
  std::uint32_t physical_adapter = 0u;
  av::DxgkPhysicalAdapterCapsRecord caps;
  std::memset(&caps, 0xEE, sizeof caps);
  int handle = 0;
  const DxgkargQueryAdapterInfoRecord record = request(&physical_adapter, 4u, &caps, sizeof caps);
  CHECK_STATUS(capabilities.report_physical_adapter_caps(AdapterInfoQuery(platform, record), &handle), Status::ok);
  /* Node 0 runs packets and pages; node 1 is the hardware-scheduled one a
   * process holds queues and doorbells on (feature 049, research.md §16). */
  CHECK_EQ(caps.NumExecutionNodes, 2u);
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

/* Feature 049: what this driver's native fences need of the OS. One monitored
 * value to a fence - none of them are packed together here - and nothing
 * mapped into a GPU system process, because this device reads no fence memory
 * of its own (research.md §14). The caps are answered at all only because the
 * four native fence DDIs are implemented (constitution VI). */
TEST(driver_capabilities_report_native_fence_caps, asks_for_one_value_a_fence_and_no_gpu_mapping)
{ MockPlatform platform; DriverCapabilities capabilities(platform);
  av::DxgkNativeFenceCapsRecord caps;
  std::memset(&caps, 0xA5, sizeof caps);
  const DxgkargQueryAdapterInfoRecord record = request(nullptr, 0u, &caps, sizeof caps);
  /* Until the OS says the feature is on, there are none to report and none of
   * NativeGpuFence is claimed either. */
  CHECK(!capabilities.native_fences_reportable());
  DxgkDriverCapsRecord driver_caps = {};
  const DxgkargQueryAdapterInfoRecord all = request(nullptr, 0u, &driver_caps, sizeof driver_caps);
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, all)), Status::ok);
  CHECK_EQ(driver_caps.SchedulingCaps & 0x800u, 0u);
  capabilities.native_fences_allowed();
  CHECK(capabilities.native_fences_reportable());
  CHECK_STATUS(capabilities.report_driver_caps(AdapterInfoQuery(platform, all)), Status::ok);
  CHECK_EQ(driver_caps.SchedulingCaps & 0x800u, 0x800u);
  CHECK_STATUS(capabilities.report_native_fence_caps(AdapterInfoQuery(platform, record)), Status::ok);
  CHECK_EQ(caps.MonitoredValueStride, sizeof(std::uint64_t));
  CHECK_EQ(caps.MapToGpuSystemProcess, 0u);
  CHECK_EQ(caps.MinimumAddress, 0u);
  CHECK_EQ(caps.MaximumAddress, 0u);
  for (const std::uint8_t byte : caps.Reserved) CHECK_EQ(byte, 0u);

  /* A buffer with no room for them is not answered with part of them. */
  const DxgkargQueryAdapterInfoRecord short_output = request(nullptr, 0u, &caps, sizeof caps - 1u);
  CHECK(capabilities.report_native_fence_caps(AdapterInfoQuery(platform, short_output)) != Status::ok);
  platform.set_irql(av::Irql::dispatch);
  static_cast<void>(capabilities.report_native_fence_caps(AdapterInfoQuery(platform, record)));
  platform.set_irql(av::Irql::passive);
  while (platform.claim(av::Violation::irql_ceiling)) {} }
