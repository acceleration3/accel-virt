#include "../../src/core/adapter.hpp"
#include "../../src/core/allocation.hpp"
#include "../../src/core/allocation_table.hpp"
#include "../../src/core/aperture_segment.hpp"
#include "../../src/core/ddi/dxgk_allocation_info_record.hpp"
#include "../../src/core/ddi/dxgk_open_allocation_info_record.hpp"
#include "../../src/core/ddi/dxgkarg_close_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_allocation_record.hpp"
#include "../../src/core/ddi/dxgkarg_create_device_record.hpp"
#include "../../src/core/ddi/dxgkarg_open_allocation_record.hpp"
#include "../../src/core/device.hpp"
#include "../../src/core/device_allocation.hpp"
#include "../../src/core/device_table.hpp"
#include "../../src/core/dxgkrnl_interface.hpp"
#include "../../src/core/status.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../harness/fake_dxgkrnl_adapter.hpp"
#include "../harness/fake_handle_table.hpp"
#include "../harness/fake_virtio_gpu_function.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"
#include "../harness/violation.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct ObjectPoolProbe {
    template <typename T>
    static void corrupt_live_count(ObjectPool<T> &pool) { pool.live_ += 1u; }
};

struct AllocationProbe {
    static void forget_opens(Allocation &allocation) { allocation.opens_ = 0u; }
};

struct DeviceTableProbe {
    static void corrupt_devices(DeviceTable &table) { ObjectPoolProbe::corrupt_live_count(table.devices_); }
    static void corrupt_opens(DeviceTable &table) { ObjectPoolProbe::corrupt_live_count(table.opens_); }
    static void drop_device(DeviceTable &table, void *device)
    { static_cast<void>(table.devices_.destroy(static_cast<Device *>(device))); }
};

}  // namespace av

using av::Adapter;
using av::Allocation;
using av::AllocationDescriptor;
using av::AllocationKind;
using av::AllocationTable;
using av::ApertureSegment;
using av::Device;
using av::DeviceTable;
using av::DxgkAllocationInfoRecord;
using av::DxgkOpenAllocationInfoRecord;
using av::DxgkargCloseAllocationRecord;
using av::DxgkargCreateAllocationRecord;
using av::DxgkargCreateDeviceRecord;
using av::DxgkargOpenAllocationRecord;
using av::DxgkrnlInterface;
using av::FakeDxgkrnlAdapter;
using av::FakeHandleTable;
using av::FakeVirtioGpuFunction;
using av::Irql;
using av::MockPlatform;
using av::ScanoutTarget;
using av::Status;
using av::SurfaceLayout;
using av::Violation;

namespace {

constexpr std::uint32_t allocation_count = 4u;

/* An adapter's tables with dxgkrnl's handle table: allocations already created
 * and each given a D3DKMT handle, as dxgkrnl does before OpenAllocation. */
class Fixture {
public:
    Fixture()
        : dxgkrnl_(platform_, function_), interface_(platform_), adapter_(platform_, &pdo_), segment_(platform_),
          allocations_(platform_, segment_), devices_(platform_, allocations_, interface_), infos_{}, descriptors_{},
          handles_{}, opens_{}
    {
        const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u), 400u,
                                          ScanoutTarget());
        DxgkargCreateAllocationRecord request = {};
        request.NumAllocations = allocation_count;
        request.pAllocationInfo = infos_;
        for (std::uint32_t i = 0u; i < allocation_count; i++) {
            static_cast<void>(shadow.encode(descriptors_[i], AllocationDescriptor::encoded_size));
            infos_[i].pPrivateDriverData = descriptors_[i];
            infos_[i].PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        }
        const bool ready = interface_.initialize(&dxgkrnl_.record()) == Status::ok &&
                           segment_.initialize(std::int64_t{1} << 30u) == Status::ok &&
                           allocations_.create(request) == Status::ok;
        if (!ready) av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "fixture");
        for (std::uint32_t i = 0u; i < allocation_count; i++) handles_[i] = kernel_handles_.insert(infos_[i].hAllocation);
    }

    void *create_device()
    {
        DxgkargCreateDeviceRecord request = {};
        request.hDevice = &runtime_device_;
        if (devices_.create(adapter_, request) != Status::ok)
            av::TestRegistry::instance().record_failure(__FILE__, __LINE__, "create device");
        return request.hDevice;
    }

    DxgkargOpenAllocationRecord open_request(std::uint32_t count)
    {
        DxgkargOpenAllocationRecord request = {};
        request.NumAllocations = count;
        request.pOpenAllocation = opens_;
        for (std::uint32_t i = 0u; i < allocation_count; i++) {
            std::memset(&opens_[i], 0, sizeof opens_[i]);
            opens_[i].hAllocation = handles_[i];
        }
        return request;
    }

    Status close(void *device, std::uint32_t count)
    {
        void *list[allocation_count] = {};
        for (std::uint32_t i = 0u; i < count; i++) list[i] = opens_[i].hDeviceSpecificAllocation;
        DxgkargCloseAllocationRecord request = {};
        request.NumAllocations = count;
        request.pOpenHandleList = list;
        return devices_.close(device, request);
    }

    Allocation *allocation(std::uint32_t i) { return allocations_.find(infos_[i].hAllocation); }

    MockPlatform                 &platform() { return platform_; }
    Adapter                      &adapter() { return adapter_; }
    AllocationTable              &allocations() { return allocations_; }
    DeviceTable                  &devices() { return devices_; }
    DxgkOpenAllocationInfoRecord &open_info(std::uint32_t i) { return opens_[i]; }
    FakeHandleTable              &kernel_handles() { return kernel_handles_; }
    std::uint32_t                 handle(std::uint32_t i) const { return handles_[i]; }

private:
    MockPlatform                 platform_;
    FakeVirtioGpuFunction        function_;
    FakeDxgkrnlAdapter           dxgkrnl_;
    FakeHandleTable              kernel_handles_;
    DxgkrnlInterface             interface_;
    int                          pdo_ = 0;
    int                          runtime_device_ = 0;
    Adapter                      adapter_;
    ApertureSegment              segment_;
    AllocationTable              allocations_;
    DeviceTable                  devices_;
    DxgkAllocationInfoRecord     infos_[allocation_count];
    std::uint8_t                 descriptors_[allocation_count][AllocationDescriptor::encoded_size];
    std::uint32_t                handles_[allocation_count];
    DxgkOpenAllocationInfoRecord opens_[allocation_count];
};

}  // namespace

/* ---- create and destroy -------------------------------------------------------- */

TEST(device_table_create, answers_a_live_device_that_knows_its_adapter)
{ Fixture fixture;
  void *handle = fixture.create_device();
  CHECK(fixture.devices().invariants_hold());
  CHECK_EQ(fixture.devices().devices(), 1u);
  const auto *device = static_cast<Device *>(handle);
  CHECK(&device->adapter() == &fixture.adapter());
  CHECK(fixture.create_device() != handle);
  CHECK(fixture.devices().find(handle) == device);
  CHECK(fixture.devices().find(&fixture) == nullptr); }

TEST(device_table_create, propagates_allocation_failure)
{ Fixture fixture;
  DxgkargCreateDeviceRecord request = {};
  request.hDevice = &fixture;
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(fixture.devices().create(fixture.adapter(), request), Status::no_memory);
  CHECK(request.hDevice == &fixture);
  CHECK_EQ(fixture.devices().devices(), 0u); }

TEST(device_table_destroy, releases_the_device_and_closes_what_it_had_open)
{ Fixture fixture;
  void *device = fixture.create_device();
  void *other = fixture.create_device();
  DxgkargOpenAllocationRecord request = fixture.open_request(2u);
  CHECK_STATUS(fixture.devices().open(device, request), Status::ok);
  DxgkargOpenAllocationRecord other_request = fixture.open_request(1u);
  CHECK_STATUS(fixture.devices().open(other, other_request), Status::ok);
  CHECK_EQ(fixture.allocation(0u)->opens(), 2u);
  CHECK_STATUS(fixture.devices().destroy(device), Status::ok);
  CHECK(fixture.devices().invariants_hold());
  CHECK_EQ(fixture.allocation(0u)->opens(), 1u);
  CHECK_EQ(fixture.allocation(1u)->opens(), 0u);
  CHECK_EQ(fixture.devices().opens(), 1u);
  CHECK_STATUS(fixture.devices().destroy(device), Status::invalid_parameter); }

/* ---- open and close ------------------------------------------------------------ */

TEST(device_table_open, resolves_each_kernel_handle_and_answers_a_per_device_handle)
{ Fixture fixture;
  void *device = fixture.create_device();
  DxgkargOpenAllocationRecord request = fixture.open_request(allocation_count);
  CHECK_STATUS(fixture.devices().open(device, request), Status::ok);
  CHECK(fixture.devices().invariants_hold());
  CHECK_EQ(fixture.devices().opens(), allocation_count);
  CHECK_EQ(fixture.kernel_handles().lookups(), allocation_count);
  CHECK(request.SubresourceOffset == 0u);
  CHECK_EQ(request.Pitch, 40u);
  for (std::uint32_t i = 0u; i < allocation_count; i++) {
      CHECK(fixture.open_info(i).hDeviceSpecificAllocation != nullptr);
      CHECK_EQ(fixture.allocation(i)->opens(), 1u);
  } }

TEST(device_table_open, refuses_the_whole_list_and_undoes_what_it_opened)
{ for (int knob = 0; knob < 6; knob++) {
      Fixture fixture;
      void *device = fixture.create_device();
      DxgkargOpenAllocationRecord request = fixture.open_request(allocation_count);
      void *target = device;
      if (knob == 0) target = &fixture;                                    /* not a device */
      if (knob == 1) request.NumAllocations = 0u;
      if (knob == 2) request.NumAllocations = AllocationTable::max_per_call + 1u;
      if (knob == 3) request.pOpenAllocation = nullptr;
      if (knob == 4) fixture.open_info(3u).hAllocation = 0x7FFFFFFFu;     /* dxgkrnl knows no such handle */
      if (knob == 5) {                                                      /* not one of our allocations */
          fixture.kernel_handles().remove(fixture.handle(3u));
          fixture.open_info(3u).hAllocation = fixture.kernel_handles().insert(&fixture);
      }
      const Status status = fixture.devices().open(target, request);
      CHECK(status == Status::invalid_parameter || status == Status::not_found);
      CHECK_EQ(fixture.devices().opens(), 0u);
      for (std::uint32_t i = 0u; i < allocation_count; i++) CHECK_EQ(fixture.allocation(i)->opens(), 0u);
      CHECK(fixture.devices().invariants_hold());
  } }

TEST(device_table_open, each_allocation_failing_in_turn_leaves_nothing_open)
{ Fixture fixture;
  void *device = fixture.create_device();
  fixture.platform().fail_allocation_at(1u);
  DxgkargOpenAllocationRecord request = fixture.open_request(allocation_count);
  CHECK_STATUS(fixture.devices().open(device, request), Status::no_memory);
  fixture.platform().fail_allocation_at(0u);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK_EQ(fixture.allocation(0u)->opens(), 0u); }

TEST(device_table_close, releases_the_listed_opens)
{ Fixture fixture;
  void *device = fixture.create_device();
  DxgkargOpenAllocationRecord request = fixture.open_request(allocation_count);
  CHECK_STATUS(fixture.devices().open(device, request), Status::ok);
  CHECK_STATUS(fixture.close(device, 2u), Status::ok);
  CHECK(fixture.devices().invariants_hold());
  CHECK_EQ(fixture.devices().opens(), 2u);
  CHECK_EQ(fixture.allocation(0u)->opens(), 0u);
  CHECK_EQ(fixture.allocation(3u)->opens(), 1u); }

TEST(device_table_close, refuses_a_list_with_a_stranger_another_devices_open_or_a_repeat)
{ Fixture fixture;
  void *device = fixture.create_device();
  void *other = fixture.create_device();
  DxgkargOpenAllocationRecord request = fixture.open_request(2u);
  CHECK_STATUS(fixture.devices().open(device, request), Status::ok);
  CHECK_STATUS(fixture.close(other, 2u), Status::invalid_parameter);
  CHECK_STATUS(fixture.close(&fixture, 2u), Status::invalid_parameter);
  fixture.open_info(1u).hDeviceSpecificAllocation = fixture.open_info(0u).hDeviceSpecificAllocation;
  CHECK_STATUS(fixture.close(device, 2u), Status::invalid_parameter);
  fixture.open_info(1u).hDeviceSpecificAllocation = &fixture;
  CHECK_STATUS(fixture.close(device, 2u), Status::invalid_parameter);
  DxgkargCloseAllocationRecord empty = {};
  empty.NumAllocations = 1u;
  CHECK_STATUS(fixture.devices().close(device, empty), Status::invalid_parameter);
  CHECK_EQ(fixture.devices().opens(), 2u); }

TEST(device_table, is_passive_only)
{ Fixture fixture;
  DxgkargCreateDeviceRecord request = {};
  fixture.platform().set_irql(Irql::dispatch);
  static_cast<void>(fixture.devices().create(fixture.adapter(), request));
  fixture.platform().set_irql(Irql::passive);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  while (fixture.platform().claim(Violation::irql_ceiling)) {}
  while (fixture.platform().claim(Violation::blocking_at_dispatch)) {}
  while (fixture.platform().claim(Violation::allocation_at_dispatch)) {} }

TEST(device_table_close, an_empty_list_closes_nothing)
{ Fixture fixture;
  void *device = fixture.create_device();
  DxgkargCloseAllocationRecord empty = {};
  CHECK_STATUS(fixture.devices().close(device, empty), Status::ok); }

TEST(device_table_invariants, detect_broken_pools_orphaned_opens_and_uncounted_opens)
{ for (int knob = 0; knob < 4; knob++) {
      Fixture fixture;
      void *device = fixture.create_device();
      DxgkargOpenAllocationRecord request = fixture.open_request(1u);
      CHECK_STATUS(fixture.devices().open(device, request), Status::ok);
      if (knob == 0) av::DeviceTableProbe::corrupt_devices(fixture.devices());
      if (knob == 1) av::DeviceTableProbe::corrupt_opens(fixture.devices());
      if (knob == 2) av::DeviceTableProbe::drop_device(fixture.devices(), device);
      if (knob == 3) av::AllocationProbe::forget_opens(*fixture.allocation(0u));
      CHECK(!fixture.devices().invariants_hold());
  } }
