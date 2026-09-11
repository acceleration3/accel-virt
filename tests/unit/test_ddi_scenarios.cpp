/* The driver image, registered and driven through its DDI table the way
 * dxgkrnl drives it (docs/ddi-sequences.md), against fake virtio-gpu
 * functions. Nothing here calls a core class directly. */
#include "../../src/core/ddi/d3dkmdt_shared_primary_surface_data_record.hpp"
#include "../../src/core/ddi/dxgk_allocation_info_record.hpp"
#include "../../src/core/ddi/dxgk_open_allocation_info_record.hpp"
#include "../../src/core/ddi/dxgkarg_close_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_device_record.hpp"
#include "../../src/core/ddi/dxgkarg_describe_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_get_standard_allocation_driver_data_record.hpp"
#include "../../src/core/ddi/dxgkarg_open_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_set_display_private_driver_format_record.hpp"
#include "../../src/core/ddi/dxgk_driver_caps_record.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../../src/core/ddi/dxgkarg_collect_dbg_info_record.hpp"
#include "../../src/core/ddi/dxgkarg_escape_record.hpp"
#include "../../src/core/ddi/dxgkarg_query_adapter_info_record.hpp"
#include "../../src/core/ddi_slot.hpp"
#include "../../src/core/ddi_table.hpp"
#include "../../src/core/driver_image.hpp"
#include "../../src/core/status.hpp"
#include "../harness/capability_rule.hpp"
#include "../harness/fake_dxgkrnl.hpp"
#include "../harness/fake_dxgkrnl_adapter.hpp"
#include "../harness/fake_dxgkrnl_fdo.hpp"
#include "../harness/fake_handle_table.hpp"
#include "../harness/fake_vidmm.hpp"
#include "../harness/fake_vidsch.hpp"
#include "../harness/fake_virtio_gpu_function.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::CapabilityRule;
using av::DdiRoutine;
using av::DdiSlot;
using av::DdiTable;
using av::DriverImage;
using av::FakeDxgkrnl;
using av::FakeDxgkrnlAdapter;
using av::FakeDxgkrnlFdo;
using av::FakeVirtioGpuFunction;
using av::MockPlatform;
using av::Status;

namespace {

constexpr std::uint32_t no_memory = 0xC0000017u;
constexpr std::uint32_t not_supported = 0xC00000BBu;
constexpr std::uint32_t invalid_device_state = 0xC0000184u;
constexpr std::int32_t  power_d0 = 1;
constexpr std::int32_t  power_d3 = 4;
constexpr std::uint8_t  driver_ok_status = 0x0Fu;

int driver_object = 0;
int registry_path = 0;

std::uint32_t nt(std::int32_t status) { return static_cast<std::uint32_t>(status); }

/* DriverEntry has run and dxgkrnl accepted the table; teardown unloads and
 * leaves the image the way the kernel does when the driver is unloaded. */
class LoadedDriver {
public:
    LoadedDriver() : entered_(image_.enter(&driver_object, &registry_path)) {}
    ~LoadedDriver()
    {
        reinterpret_cast<void (*)()>(table().routine(DdiSlot::DxgkDdiUnload))();
        image_.leave();
    }

    int32_t         entered() const { return entered_; }
    MockPlatform   &platform() { return image_.platform(); }
    const DdiTable &table() { return *image_.platform().registered_table(); }

private:
    DriverImage<MockPlatform> image_;
    int32_t                   entered_;
};

/* One PCI function dxgkrnl found, with its FDO. */
class Device {
public:
    Device(LoadedDriver &driver, std::uint8_t index)
        : dxgkrnl_(driver.platform(), function_, index), fdo_(driver.platform(), driver.table(), dxgkrnl_) {}

    FakeVirtioGpuFunction &function() { return function_; }
    FakeDxgkrnlAdapter    &dxgkrnl() { return dxgkrnl_; }
    FakeDxgkrnlFdo        &fdo() { return fdo_; }

private:
    FakeVirtioGpuFunction function_;
    FakeDxgkrnlAdapter    dxgkrnl_;
    FakeDxgkrnlFdo        fdo_;
};

}  // namespace

TEST(ddi_scenario, the_recorded_bring_up_and_teardown)
{ LoadedDriver driver;
  CHECK_EQ(driver.entered(), 0);
  FakeDxgkrnl dxgkrnl;
  CHECK_STATUS(dxgkrnl.initialize(&driver.table()), Status::ok);
  { Device device(driver, 0u);
    CHECK_EQ(device.fdo().add_device(), 0);
    CHECK(device.fdo().context() != nullptr);
    CHECK_EQ(device.fdo().start_device(), 0);
    CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::none);
    CHECK_EQ(device.fdo().dxgadapter().driver_caps().WDDMVersion, 0x1000);
    CHECK_EQ(device.fdo().sources(), 1u);
    CHECK_EQ(device.fdo().children(), 1u);
    CHECK_EQ(device.function().common().status(), driver_ok_status);
    CHECK_EQ(device.fdo().enumerate_children(), 0);
    CHECK_EQ(device.fdo().connected_children(), 1u);
    CHECK_EQ(device.fdo().interrupt(1u), 1u);
    CHECK_EQ(device.fdo().set_power_state(power_d3), 0);
    CHECK_EQ(device.function().common().status(), 0u);
    CHECK_EQ(device.fdo().set_power_state(power_d0), 0);
    CHECK_EQ(device.function().common().status(), driver_ok_status);
    CHECK_EQ(device.fdo().stop_device(), 0);
    CHECK_EQ(device.function().common().status(), 0u);
    CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
    CHECK_EQ(device.fdo().remove_device(), 0); }
  CHECK_EQ(driver.platform().live_allocations(), 0u);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, stop_device_twice)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  CHECK_EQ(device.fdo().stop_device(), 0);
  CHECK_EQ(device.fdo().stop_device(), 0);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, remove_device_without_stop_device)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  const std::int32_t remove = reinterpret_cast<std::int32_t (*)(void *)>(
      driver.table().routine(DdiSlot::DxgkDdiRemoveDevice))(device.fdo().context());
  CHECK_EQ(remove, 0);
  CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(device.function().common().status(), 0u);
  CHECK_EQ(driver.platform().live_allocations(), 0u); }

TEST(ddi_scenario, an_interrupt_before_start_device_finished)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().interrupt(0u), 0u);
  CHECK_EQ(device.dxgkrnl().dpcs_queued(), 0u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_failed_start_is_stopped_then_removed)
{ LoadedDriver driver;
  Device device(driver, 0u);
  device.function().common().offer_features(FakeVirtioGpuFunction::feature_edid);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(nt(device.fdo().start_device()), not_supported);
  CHECK(!device.fdo().started());
  CHECK_EQ(device.dxgkrnl().live_mappings(), 0u);
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), 0u); }

TEST(ddi_scenario, reset_device_at_bugcheck_then_teardown)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  const std::uint32_t allocations = driver.platform().allocations_made();
  device.fdo().reset_device();
  CHECK_EQ(device.function().common().status(), 0u);
  CHECK_EQ(driver.platform().allocations_made(), allocations);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, one_adapter_per_host_gpu)
{ LoadedDriver driver;
  Device first(driver, 0u);
  Device second(driver, 1u);
  CHECK_EQ(first.fdo().add_device(), 0);
  CHECK_EQ(second.fdo().add_device(), 0);
  CHECK(first.fdo().context() != second.fdo().context());
  CHECK_EQ(first.fdo().start_device(), 0);
  CHECK_EQ(second.fdo().start_device(), 0);
  CHECK_EQ(first.fdo().interrupt(1u), 1u);
  CHECK_EQ(second.dxgkrnl().dpcs_queued(), 0u);
  CHECK_EQ(first.fdo().remove_device(), 0);
  CHECK_EQ(second.function().common().status(), driver_ok_status);
  CHECK_EQ(second.fdo().remove_device(), 0); }

TEST(ddi_scenario, add_device_past_the_adapter_ceiling_is_no_memory)
{ LoadedDriver driver;
  FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(driver.platform(), function);
  FakeDxgkrnlFdo fdos[] = {
      {driver.platform(), driver.table(), dxgkrnl}, {driver.platform(), driver.table(), dxgkrnl},
      {driver.platform(), driver.table(), dxgkrnl}, {driver.platform(), driver.table(), dxgkrnl},
      {driver.platform(), driver.table(), dxgkrnl}, {driver.platform(), driver.table(), dxgkrnl},
      {driver.platform(), driver.table(), dxgkrnl}, {driver.platform(), driver.table(), dxgkrnl},
      {driver.platform(), driver.table(), dxgkrnl},
  };
  for (std::uint32_t i = 0u; i < 8u; i++) CHECK_EQ(fdos[i].add_device(), 0);
  CHECK_EQ(nt(fdos[8].add_device()), no_memory);
  CHECK(fdos[8].context() == nullptr);
  CHECK_EQ(driver.platform().logged(), 2u);   /* DriverEntry, and the refusal */ }

/* ---- Capability reporting (feature 007) ------------------------------------ */

namespace {

constexpr std::int32_t revision_mismatch = static_cast<std::int32_t>(0xC0000059u);

/* The driver's own table with QueryAdapterInfo answering an unknown WDDM
 * version, as a later feature's mistake would. */
std::int32_t answers_an_unknown_wddm_version(void *, const av::DxgkargQueryAdapterInfoRecord *query)
{
    if (query->Type != 1) return static_cast<std::int32_t>(0xC0000002u);
    av::DxgkDriverCapsRecord caps = {};
    caps.WDDMVersion = 0x1234;
    std::memcpy(query->pOutputData, &caps, sizeof caps);
    return 0;
}

}  // namespace

TEST(ddi_scenario, every_query_adapter_info_type_answers_a_status_dxgkrnl_accepts)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  std::uint8_t input[0x250] = {}, output[0x250] = {};
  for (std::int32_t type = -1; type <= 49; type++)
      static_cast<void>(device.fdo().dxgadapter().query(device.fdo().context(), type, input, sizeof input,
                                                        output, sizeof output));
  CHECK_EQ(device.fdo().start_device(), 0);
  for (std::int32_t type = -1; type <= 49; type++)
      static_cast<void>(device.fdo().dxgadapter().query(device.fdo().context(), type, input, 0u, output, 0u));
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_refused_capability_fails_start_and_stops_the_device)
{ LoadedDriver driver;
  DdiTable table;
  for (std::uint16_t index = 1u; index < DdiTable::slot_count; index++)
      CHECK_STATUS(table.set(DdiSlot{index}, driver.table().routine(DdiSlot{index})), Status::ok);
  CHECK_STATUS(table.set(DdiSlot::DxgkDdiQueryAdapterInfo,
                         reinterpret_cast<DdiRoutine>(&answers_an_unknown_wddm_version)), Status::ok);
  FakeVirtioGpuFunction function;
  FakeDxgkrnlAdapter dxgkrnl(driver.platform(), function);
  FakeDxgkrnlFdo fdo(driver.platform(), table, dxgkrnl);
  CHECK_EQ(fdo.add_device(), 0);
  CHECK_EQ(fdo.start_device(), revision_mismatch);
  CHECK(fdo.dxgadapter().failed_rule() == CapabilityRule::unknown_wddm_version);
  CHECK(!fdo.started());
  CHECK_EQ(function.common().status(), 0u);
  CHECK_EQ(dxgkrnl.live_mappings(), 0u);
  CHECK_EQ(fdo.remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), 0u); }

TEST(ddi_scenario, escape_and_debug_collection_through_the_table)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  std::uint8_t data[16] = {};
  av::DxgkargEscapeRecord escape = {};
  escape.pPrivateDriverData = data;
  escape.PrivateDriverDataSize = sizeof data;
  const auto escape_routine = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargEscapeRecord *)>(
      driver.table().routine(DdiSlot::DxgkDdiEscape));
  CHECK_EQ(nt(escape_routine(device.fdo().context(), &escape)), not_supported);
  std::uint8_t report[16] = {};
  av::DxgkargCollectDbgInfoRecord collect = {};
  collect.pBuffer = report;
  collect.BufferSize = sizeof report;
  const auto collect_routine = reinterpret_cast<std::int32_t (*)(void *, const av::DxgkargCollectDbgInfoRecord *)>(
      driver.table().routine(DdiSlot::DxgkDdiCollectDbgInfo));
  CHECK_EQ(collect_routine(device.fdo().context(), &collect), 0);
  CHECK_EQ(report[6], 1u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

/* ---- Memory model (feature 008) ---------------------------------------------- */

TEST(ddi_scenario, vidmm_accepts_the_aperture_segment_after_start)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  av::FakeVidMm vidmm(device.fdo().dxgadapter());
  CHECK_EQ(vidmm.create_segments(device.fdo().context()), 0);
  CHECK(vidmm.failed_rule() == av::SegmentRule::none);
  CHECK(vidmm.segment(0u).Size == std::uint64_t{8} << 30u);
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, the_render_ddi_set_is_incomplete_until_present_arrives)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  CHECK_EQ(device.fdo().dxgadapter().create_render_core(), revision_mismatch);
  CHECK(device.fdo().dxgadapter().failed_rule() == CapabilityRule::render_ddis_incomplete);
  CHECK(device.fdo().dxgadapter().missing_render_slot() == DdiSlot::DxgkDdiControlInterrupt);
  CHECK_EQ(device.fdo().remove_device(), 0); }

namespace {

/* The runtime side of dxgkrnl for one adapter: the calls it makes to create a
 * primary for DWM and open it on a device, through the registered table. */
class Runtime {
public:
    Runtime(const DdiTable &table, void *adapter) : table_(table), adapter_(adapter), surface_{1024u, 768u, 21, 60u, 1u, 0u},
        descriptor_{}, info_{}, open_{}, device_(nullptr) {}

    template <typename Routine>
    Routine routine(DdiSlot slot) const { return reinterpret_cast<Routine>(table_.routine(slot)); }

    std::int32_t create_primary()
    {
        av::DxgkargGetStandardAllocationDriverDataRecord describe = {};
        describe.StandardAllocationType = 1;
        describe.pCreateSurfaceData = &surface_;
        std::int32_t status = routine<std::int32_t (*)(void *, av::DxgkargGetStandardAllocationDriverDataRecord *)>(
            DdiSlot::DxgkDdiGetStandardAllocationDriverData)(adapter_, &describe);
        if (status < 0) return status;
        describe.pAllocationPrivateDriverData = descriptor_;
        status = routine<std::int32_t (*)(void *, av::DxgkargGetStandardAllocationDriverDataRecord *)>(
            DdiSlot::DxgkDdiGetStandardAllocationDriverData)(adapter_, &describe);
        if (status < 0) return status;
        info_.pPrivateDriverData = descriptor_;
        info_.PrivateDriverDataSize = describe.AllocationPrivateDriverDataSize;
        av::DxgkargCreateAllocationRecord create = {};
        create.NumAllocations = 1u;
        create.pAllocationInfo = &info_;
        status = routine<std::int32_t (*)(void *, av::DxgkargCreateAllocationRecord *)>(DdiSlot::DxgkDdiCreateAllocation)(
            adapter_, &create);
        if (status >= 0) kernel_handle_ = handles_.insert(info_.hAllocation);
        return status;
    }

    std::int32_t create_device()
    {
        av::DxgkargCreateDeviceRecord create = {};
        const std::int32_t status =
            routine<std::int32_t (*)(void *, av::DxgkargCreateDeviceRecord *)>(DdiSlot::DxgkDdiCreateDevice)(adapter_, &create);
        device_ = create.hDevice;
        return status;
    }

    std::int32_t open()
    {
        open_.hAllocation = kernel_handle_;
        av::DxgkargOpenAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pOpenAllocation = &open_;
        return routine<std::int32_t (*)(void *, av::DxgkargOpenAllocationRecord *)>(DdiSlot::DxgkDdiOpenAllocation)(device_, &request);
    }

    std::int32_t describe(av::DxgkargDescribeAllocationRecord &request)
    {
        request.hAllocation = info_.hAllocation;
        return routine<std::int32_t (*)(void *, av::DxgkargDescribeAllocationRecord *)>(DdiSlot::DxgkDdiDescribeAllocation)(
            adapter_, &request);
    }

    std::int32_t set_private_format(std::uint32_t attribute)
    {
        av::DxgkargSetDisplayPrivateDriverFormatRecord request = {};
        request.PrimaryAllocation = info_.hAllocation;
        request.PrivateDriverFormatAttribute = attribute;
        return routine<std::int32_t (*)(void *, const av::DxgkargSetDisplayPrivateDriverFormatRecord *)>(
            DdiSlot::DxgkDdiSetDisplayPrivateDriverFormat)(adapter_, &request);
    }

    std::int32_t close()
    {
        void *handle = open_.hDeviceSpecificAllocation;
        av::DxgkargCloseAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pOpenHandleList = &handle;
        return routine<std::int32_t (*)(void *, const av::DxgkargCloseAllocationRecord *)>(DdiSlot::DxgkDdiCloseAllocation)(
            device_, &request);
    }

    std::int32_t destroy_primary()
    {
        void *handle = info_.hAllocation;
        av::DxgkargDestroyAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationList = &handle;
        return routine<std::int32_t (*)(void *, const av::DxgkargDestroyAllocationRecord *)>(DdiSlot::DxgkDdiDestroyAllocation)(
            adapter_, &request);
    }

    std::int32_t destroy_device()
    { return routine<std::int32_t (*)(void *)>(DdiSlot::DxgkDdiDestroyDevice)(device_); }

private:
    const DdiTable                            &table_;
    void                                      *adapter_;
    av::D3dkmdtSharedPrimarySurfaceDataRecord  surface_;
    std::uint8_t                               descriptor_[av::AllocationDescriptor::encoded_size];
    av::DxgkAllocationInfoRecord               info_;
    av::DxgkOpenAllocationInfoRecord           open_;
    void                                      *device_;
    av::FakeHandleTable                        handles_;
    std::uint32_t                              kernel_handle_ = 0u;
};

}  // namespace

TEST(ddi_scenario, a_primary_lives_from_standard_allocation_to_destruction)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { Runtime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.create_device(), 0);
    CHECK_EQ(runtime.open(), 0);
    CHECK_EQ(runtime.set_private_format(0x42u), 0);
    av::DxgkargDescribeAllocationRecord described = {};
    CHECK_EQ(runtime.describe(described), 0);
    CHECK_EQ(described.Width, 1024u);
    CHECK_EQ(described.RefreshRateNumerator, 60u);
    CHECK_EQ(described.PrivateDriverFormatAttribute, 0x42u);
    CHECK_EQ(runtime.close(), 0);
    CHECK_EQ(runtime.destroy_primary(), 0);
    CHECK_EQ(runtime.destroy_device(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), 0u);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, destroy_device_with_an_open_then_the_allocation)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { Runtime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.create_device(), 0);
    CHECK_EQ(runtime.open(), 0);
    CHECK_EQ(nt(runtime.destroy_primary()), invalid_device_state);
    CHECK_EQ(runtime.destroy_device(), 0);
    CHECK_EQ(runtime.destroy_primary(), 0); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), 0u); }

TEST(ddi_scenario, remove_device_with_devices_and_allocations_outstanding)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { Runtime runtime(driver.table(), device.fdo().context());
    CHECK_EQ(runtime.create_primary(), 0);
    CHECK_EQ(runtime.create_device(), 0);
    CHECK_EQ(runtime.open(), 0);
    CHECK_EQ(device.fdo().remove_device(), 0); }
  CHECK_EQ(driver.platform().live_allocations(), 0u); }

/* ---- Submission (feature 009) ------------------------------------------------- */

TEST(ddi_scenario, render_patch_submit_and_three_fences_complete_through_the_dpc)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(), 0);
    std::uint8_t command[32] = {0x52, 0x56, 0x4B, 0x53};
    for (int packet = 0; packet < 3; packet++) {
        CHECK_EQ(vidsch.render(command, sizeof command, 8u, 0xC0002000 + static_cast<std::int64_t>(packet)), 0);
        CHECK_EQ(vidsch.submit(), 0);
    }
    CHECK_EQ(vidsch.dma()[0], 0x52u);
    CHECK_EQ(vidsch.dma()[8], 0x02u);
    CHECK_EQ(vidsch.dma()[9], 0x20u);
    CHECK_EQ(vidsch.completed(), 0u);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 3u);
    std::uint32_t fence = 0u;
    CHECK_EQ(vidsch.query_current_fence(fence), 0);
    CHECK_EQ(fence, 3u);
    CHECK_EQ(device.dxgkrnl().dpc_notifications(), 1u); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), 0u);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }

TEST(ddi_scenario, a_preemption_is_reported_after_what_was_pending_and_again_with_nothing_pending)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(), 0);
    std::uint8_t command[16] = {};
    CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
    CHECK_EQ(vidsch.submit(), 0);
    CHECK_EQ(vidsch.preempt(), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 1u);
    CHECK_EQ(vidsch.preemptions(), 1u);
    CHECK_EQ(vidsch.preempt(), 0);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.preemptions(), 2u); }
  CHECK_EQ(device.fdo().remove_device(), 0); }

TEST(ddi_scenario, a_render_that_overruns_is_refused_and_nothing_is_submitted)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(), 0);
    std::uint8_t command[16] = {};
    CHECK_EQ(nt(vidsch.render(command, sizeof command, 9u, 0)), 0xC000000Du);
    vidsch.run_dpc();
    CHECK_EQ(vidsch.completed(), 0u); }
  CHECK_EQ(device.fdo().remove_device(), 0); }

/* ---- TDR (feature 010) ---------------------------------------------------------- */

TEST(ddi_scenario, a_timeout_is_recovered_from_twice)
{ LoadedDriver driver;
  Device device(driver, 0u);
  CHECK_EQ(device.fdo().add_device(), 0);
  CHECK_EQ(device.fdo().start_device(), 0);
  { av::FakeVidSch vidsch(driver.platform(), driver.table(), device.dxgkrnl(), device.fdo().context());
    CHECK_EQ(vidsch.create_context(), 0);
    std::uint8_t command[16] = {};
    for (int round = 0; round < 2; round++) {
        CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
        CHECK_EQ(vidsch.submit(), 0);
        CHECK_EQ(vidsch.reset_from_timeout(), 0);
        vidsch.run_dpc();
        CHECK_EQ(device.function().common().status(), 0u);
        CHECK_EQ(vidsch.restart_from_timeout(), 0);
        CHECK_EQ(device.function().common().status(), driver_ok_status);
        CHECK_EQ(vidsch.render(command, sizeof command, 0u, 0), 0);
        CHECK_EQ(vidsch.submit(), 0);
        vidsch.run_dpc();
        CHECK_EQ(vidsch.completed(), vidsch.submitted());
    }
    std::uint32_t fence = 0u;
    CHECK_EQ(vidsch.query_current_fence(fence), 0);
    CHECK_EQ(fence, 4u); }
  CHECK_EQ(device.fdo().remove_device(), 0);
  CHECK_EQ(driver.platform().live_allocations(), 0u);
  CHECK_EQ(driver.platform().unclaimed_violations(), 0u); }
