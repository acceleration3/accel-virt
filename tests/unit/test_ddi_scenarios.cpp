/* The driver image, registered and driven through its DDI table the way
 * dxgkrnl drives it (docs/ddi-sequences.md), against fake virtio-gpu
 * functions. Nothing here calls a core class directly. */
#include "../../src/core/ddi_slot.hpp"
#include "../../src/core/ddi_table.hpp"
#include "../../src/core/driver_image.hpp"
#include "../../src/core/status.hpp"
#include "../harness/fake_dxgkrnl.hpp"
#include "../harness/fake_dxgkrnl_adapter.hpp"
#include "../harness/fake_dxgkrnl_fdo.hpp"
#include "../harness/fake_virtio_gpu_function.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"

#include <cstdint>

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
