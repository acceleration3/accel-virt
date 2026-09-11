#include "../../src/core/ddi/entry_points.hpp"
#include "../../src/core/ddi_slot.hpp"
#include "../../src/core/ddi_table.hpp"
#include "../../src/core/miniport.hpp"
#include "../../src/core/miniport_binding.hpp"
#include "../../src/core/status.hpp"
#include "../harness/ddi_table_probe.hpp"
#include "../harness/fake_dxgkrnl.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

using av::DdiSlot;
using av::FakeDxgkrnl;
using av::Miniport;
using av::MiniportBinding;
using av::MockPlatform;
using av::Status;

namespace av {

struct MiniportProbe {
    explicit MiniportProbe(Miniport &target) : miniport(target) {}
    void spoil_table() { miniport.table_status_ = 1u; }
    DdiTable &table() { return miniport.table_; }
    Miniport &miniport;
};

}  // namespace av

using av::MiniportProbe;

namespace {

constexpr DdiSlot implemented[] = {
    DdiSlot::DxgkDdiAddDevice,           DdiSlot::DxgkDdiStartDevice,
    DdiSlot::DxgkDdiStopDevice,          DdiSlot::DxgkDdiRemoveDevice,
    DdiSlot::DxgkDdiDispatchIoRequest,   DdiSlot::DxgkDdiInterruptRoutine,
    DdiSlot::DxgkDdiDpcRoutine,          DdiSlot::DxgkDdiQueryChildRelations,
    DdiSlot::DxgkDdiQueryChildStatus,    DdiSlot::DxgkDdiQueryDeviceDescriptor,
    DdiSlot::DxgkDdiSetPowerState,       DdiSlot::DxgkDdiNotifyAcpiEvent,
    DdiSlot::DxgkDdiResetDevice,         DdiSlot::DxgkDdiUnload,
    DdiSlot::DxgkDdiQueryInterface,      DdiSlot::DxgkDdiControlEtwLogging,
    DdiSlot::DxgkDdiLinkDevice,          DdiSlot::DxgkDdiQueryAdapterInfo,
    DdiSlot::DxgkDdiEscape,              DdiSlot::DxgkDdiCollectDbgInfo,
    DdiSlot::DxgkDdiCreateDevice,        DdiSlot::DxgkDdiDestroyDevice,
    DdiSlot::DxgkDdiCreateAllocation,    DdiSlot::DxgkDdiDestroyAllocation,
    DdiSlot::DxgkDdiDescribeAllocation,  DdiSlot::DxgkDdiGetStandardAllocationDriverData,
    DdiSlot::DxgkDdiOpenAllocation,      DdiSlot::DxgkDdiCloseAllocation,
    DdiSlot::DxgkDdiSetDisplayPrivateDriverFormat,
    DdiSlot::DxgkDdiCreateContext,       DdiSlot::DxgkDdiDestroyContext,
    DdiSlot::DxgkDdiRender,              DdiSlot::DxgkDdiPatch,
    DdiSlot::DxgkDdiSubmitCommand,       DdiSlot::DxgkDdiPreemptCommand,
    DdiSlot::DxgkDdiQueryCurrentFence,   DdiSlot::DxgkDdiBuildPagingBuffer,
    DdiSlot::DxgkDdiResetFromTimeout,    DdiSlot::DxgkDdiRestartFromTimeout,
    DdiSlot::DxgkDdiIsSupportedVidPn,    DdiSlot::DxgkDdiRecommendFunctionalVidPn,
    DdiSlot::DxgkDdiEnumVidPnCofuncModality, DdiSlot::DxgkDdiCommitVidPn,
    DdiSlot::DxgkDdiUpdateActiveVidPnPresentPath, DdiSlot::DxgkDdiRecommendMonitorModes,
    DdiSlot::DxgkDdiRecommendVidPnTopology, DdiSlot::DxgkDdiGetScanLine,
    DdiSlot::DxgkDdiControlInterrupt,    DdiSlot::DxgkDdiSetVidPnSourceAddress,
    DdiSlot::DxgkDdiSetVidPnSourceVisibility, DdiSlot::DxgkDdiPresent,
    DdiSlot::DxgkDdiSetPointerShape,     DdiSlot::DxgkDdiSetPointerPosition,
    DdiSlot::DxgkDdiAcquireSwizzlingRange, DdiSlot::DxgkDdiReleaseSwizzlingRange,
    DdiSlot::DxgkDdiSetPalette,          DdiSlot::DxgkDdiStopCapture,
    DdiSlot::DxgkDdiCreateOverlay,       DdiSlot::DxgkDdiUpdateOverlay,
    DdiSlot::DxgkDdiFlipOverlay,         DdiSlot::DxgkDdiDestroyOverlay,
    DdiSlot::DxgkDdiQueryVidPnHWCapability,
    DdiSlot::DxgkDdiSetPowerComponentFState, DdiSlot::DxgkDdiQueryDependentEngineGroup,
    DdiSlot::DxgkDdiQueryEngineStatus,   DdiSlot::DxgkDdiResetEngine,
    DdiSlot::DxgkDdiStopDeviceAndReleasePostDisplayOwnership, DdiSlot::DxgkDdiSystemDisplayEnable,
    DdiSlot::DxgkDdiSystemDisplayWrite,  DdiSlot::DxgkDdiCancelCommand,
    DdiSlot::DxgkDdiGetChildContainerId, DdiSlot::DxgkDdiPowerRuntimeControlRequest,
    DdiSlot::DxgkDdiSetVidPnSourceAddressWithMultiPlaneOverlay, DdiSlot::DxgkDdiNotifySurpriseRemoval,
    DdiSlot::DxgkDdiGetNodeMetadata,     DdiSlot::DxgkDdiSetPowerPState,
    DdiSlot::DxgkDdiControlInterrupt2,   DdiSlot::DxgkDdiCalibrateGpuClock,
    DdiSlot::DxgkDdiSubmitCommandVirtual, DdiSlot::DxgkDdiSetRootPageTable,
    DdiSlot::DxgkDdiGetRootPageTableSize, DdiSlot::DxgkDdiCreateProcess,
    DdiSlot::DxgkDdiDestroyProcess,      DdiSlot::DxgkDdiSetStablePowerState,
    DdiSlot::DxgkDdiControlModeBehavior, DdiSlot::DxgkDdiUpdateMonitorLinkInfo,
    DdiSlot::DxgkDdiCreateHwQueue,       DdiSlot::DxgkDdiDestroyHwQueue,
    DdiSlot::DxgkDdiSubmitCommandToHwQueue, DdiSlot::DxgkDdiResetHwEngine,
    DdiSlot::DxgkDdiResumeHwEngine,
};

bool is_implemented(std::uint16_t index)
{
    for (const DdiSlot slot : implemented)
        if (static_cast<std::uint16_t>(slot) == index) return true;
    return false;
}

/* Binds a miniport for the scope and unbinds it on exit, as DriverEntry and
 * Unload do in the kernel. */
class Bound {
public:
    Bound() : miniport_(platform_) { av::miniport_binding.bind(miniport_); }
    ~Bound() { av::miniport_binding.unbind(); }
    MockPlatform &platform() { return platform_; }
    Miniport     &miniport() { return miniport_; }

private:
    MockPlatform platform_;
    Miniport     miniport_;
};

}  // namespace

TEST(miniport, fills_exactly_the_slots_this_feature_implements)
{ MockPlatform platform; Miniport miniport(platform);
  CHECK(miniport.invariants_hold());
  bool exact = true;
  for (std::uint16_t index = 1u; index < av::DdiTable::slot_count; index++)
      exact = exact && ((miniport.ddi_table().routine(DdiSlot{index}) != nullptr) == is_implemented(index));
  CHECK(exact); }

TEST(miniport, its_table_passes_dxgkrnl_registration)
{ MockPlatform platform; Miniport miniport(platform); FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(&miniport.ddi_table()), Status::ok); }

TEST(miniport_add_device, gives_each_device_its_own_context_up_to_the_limit)
{ MockPlatform platform; Miniport miniport(platform);
  void *contexts[Miniport::max_adapters] = {};
  int pdo = 0;
  for (std::uint32_t i = 0u; i < Miniport::max_adapters; i++) {
      CHECK_STATUS(miniport.add_device(&pdo, contexts[i]), Status::ok);
      CHECK(contexts[i] != nullptr);
      CHECK(miniport.invariants_hold());
  }
  CHECK(contexts[0] != contexts[1]);
  void *overflow = &pdo;
  CHECK_STATUS(miniport.add_device(&pdo, overflow), Status::no_memory);
  CHECK(overflow == nullptr);
  CHECK_EQ(miniport.adapter_count(), Miniport::max_adapters);
  for (void *context : contexts) CHECK_STATUS(miniport.remove_device(context), Status::ok);
  CHECK_EQ(miniport.adapter_count(), 0u); }

TEST(miniport_add_device, is_passive_only)
{ MockPlatform platform; Miniport miniport(platform);
  platform.set_irql(av::Irql::dispatch);
  int pdo = 0; void *context = nullptr;
  CHECK_STATUS(miniport.add_device(&pdo, context), Status::ok);
  CHECK(platform.claim(av::Violation::irql_ceiling));
  platform.set_irql(av::Irql::passive);
  CHECK_STATUS(miniport.remove_device(context), Status::ok); }

TEST(miniport_remove_device, rejects_a_context_it_did_not_create)
{ MockPlatform platform; Miniport miniport(platform);
  int stranger = 0;
  CHECK_STATUS(miniport.remove_device(&stranger), Status::invalid_parameter);
  CHECK_STATUS(miniport.remove_device(nullptr), Status::invalid_parameter);
  int pdo = 0; void *context = nullptr;
  CHECK_STATUS(miniport.add_device(&pdo, context), Status::ok);
  CHECK_STATUS(miniport.remove_device(context), Status::ok);
  CHECK_STATUS(miniport.remove_device(context), Status::invalid_parameter); }

TEST(miniport_unload, removes_every_remaining_adapter)
{ MockPlatform platform; Miniport miniport(platform);
  int pdo = 0; void *context = nullptr;
  CHECK_STATUS(miniport.add_device(&pdo, context), Status::ok);
  miniport.unload();
  CHECK_EQ(miniport.adapter_count(), 0u);
  CHECK(miniport.invariants_hold()); }

TEST(miniport_invariants, a_table_that_failed_to_build_is_detected)
{ MockPlatform platform; Miniport miniport(platform);
  MiniportProbe(miniport).spoil_table();
  CHECK(!miniport.invariants_hold()); }

TEST(miniport_binding, binds_and_unbinds_one_root)
{ MockPlatform platform; Miniport miniport(platform); MiniportBinding binding;
  CHECK(binding.miniport() == nullptr);
  binding.bind(miniport);
  CHECK(binding.miniport() == &miniport);
  binding.unbind();
  CHECK(binding.miniport() == nullptr); }

/* ---- the entry points, through the table ------------------------------------ */

TEST(ddi_entry_points, add_and_remove_device_reach_the_bound_root)
{ Bound bound;
  int pdo = 0; void *context = nullptr;
  CHECK_EQ(AvDdiAddDevice(&pdo, &context), 0);
  CHECK(context != nullptr);
  CHECK_EQ(bound.miniport().adapter_count(), 1u);
  CHECK_EQ(AvDdiRemoveDevice(context), 0);
  CHECK_EQ(bound.miniport().adapter_count(), 0u); }

TEST(ddi_entry_points, unload_reaches_the_bound_root)
{ Bound bound;
  int pdo = 0; void *context = nullptr;
  CHECK_EQ(AvDdiAddDevice(&pdo, &context), 0);
  AvDdiUnload();
  CHECK_EQ(bound.miniport().adapter_count(), 0u); }

TEST(ddi_entry_points, with_no_root_bound_fail_without_dereferencing)
{ int pdo = 0; void *context = &pdo;
  CHECK(av::miniport_binding.miniport() == nullptr);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiAddDevice(&pdo, &context)), 0xC0000184u);
  CHECK(context == nullptr);
  AvDdiUnload(); }

TEST(ddi_entry_points, each_forwards_to_the_adapter_behind_the_context)
{ Bound bound;
  int pdo = 0; void *context = nullptr;
  CHECK_EQ(AvDdiAddDevice(&pdo, &context), 0);
  std::uint32_t sources = 9u, children = 9u;
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiStartDevice(context, nullptr, nullptr, &sources, &children)), 0xC000000Du);
  CHECK_EQ(AvDdiStopDevice(context), 0);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiDispatchIoRequest(context, 0u, nullptr)), 0xC00000BBu);
  CHECK_EQ(AvDdiInterruptRoutine(context, 0u), 0u);
  AvDdiDpcRoutine(context);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiQueryChildRelations(context, nullptr, 0u)), 0xC000000Du);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiQueryChildStatus(context, nullptr, 0u)), 0xC000000Du);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiQueryDeviceDescriptor(context, 0u, nullptr)), 0xC000000Du);
  CHECK_EQ(AvDdiSetPowerState(context, 0u, 1, 0), 0);
  std::uint32_t acpi_flags = 5u;
  CHECK_EQ(AvDdiNotifyAcpiEvent(context, 0, 0u, nullptr, &acpi_flags), 0);
  CHECK_EQ(acpi_flags, 0u);
  AvDdiResetDevice(context);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiQueryInterface(context, nullptr)), 0xC00000BBu);
  AvDdiControlEtwLogging(1u, 0u, 0u);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiLinkDevice(&pdo, context, nullptr)), 0xC000000Du);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiUpdateOverlay(nullptr, nullptr)), 0xC00000BBu);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiFlipOverlay(nullptr, nullptr)), 0xC00000BBu);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiDestroyOverlay(nullptr)), 0xC00000BBu);
  CHECK_EQ(static_cast<std::uint32_t>(AvDdiSetPalette(context, nullptr)), 0xC00000BBu);
  CHECK_EQ(AvDdiRemoveDevice(context), 0); }

TEST(miniport_remove_device, finds_an_adapter_behind_another)
{ MockPlatform platform; Miniport miniport(platform);
  int pdo = 0; void *first = nullptr; void *second = nullptr;
  CHECK_STATUS(miniport.add_device(&pdo, first), Status::ok);
  CHECK_STATUS(miniport.add_device(&pdo, second), Status::ok);
  CHECK_STATUS(miniport.remove_device(second), Status::ok);
  CHECK_EQ(miniport.adapter_count(), 1u);
  CHECK_STATUS(miniport.remove_device(first), Status::ok); }

TEST(miniport_invariants, a_corrupted_table_is_detected)
{ MockPlatform platform; Miniport miniport(platform);
  av::DdiTableProbe(MiniportProbe(miniport).table()).set_padding(1u);
  CHECK(!miniport.invariants_hold()); }

TEST(miniport_binding, remove_and_unload_with_nothing_bound_fail_safely)
{ MiniportBinding binding; int context = 0;
  CHECK_EQ(static_cast<std::uint32_t>(binding.remove_device(&context)), 0xC0000184u);
  binding.unload(); }
