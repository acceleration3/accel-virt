#include "core/adapter/adapter.hpp"
#include "core/ddi/dxgk_allocation_info_record.hpp"
#include "core/ddi/dxgk_open_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_close_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_allocation_record.hpp"
#include "core/ddi/dxgkarg_create_device_record.hpp"
#include "core/ddi/dxgkarg_open_allocation_record.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/allocation_table.hpp"
#include "core/memory/shared_object_table.hpp"
#include "core/memory/host_visible_segment.hpp"
#include "core/memory/memory_segments.hpp"
#include "core/memory/aperture_segment.hpp"
#include "core/memory/device_allocation.hpp"
#include "core/miniport/dxgkrnl_interface.hpp"
#include "core/scheduling/device.hpp"
#include "core/scheduling/device_table.hpp"
#include "core/support/status.hpp"
#include "harness/device/fake_virtio_gpu_function.hpp"
#include "harness/dxgkrnl/fake_dxgkrnl_adapter.hpp"
#include "harness/dxgkrnl/fake_handle_table.hpp"
#include "core/platform/lock_rank.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"
#include "core/ddi/dxgkarg_destroy_allocation_record.hpp"
#include "proto/resources/allocation_descriptor.hpp"
#include "proto/resources/shared_object_record.hpp"

#include <cstdint>
#include <cstdio>
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
          host_visible_(platform_), segments_(platform_, segment_, host_visible_),
          shared_(platform_), allocations_(platform_, segment_, segments_, shared_), devices_(platform_, allocations_, shared_, interface_), infos_{}, descriptors_{},
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
        const bool ready = shared_.initialize() == Status::ok &&
                           interface_.initialize(&dxgkrnl_.record()) == Status::ok &&
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

    /* Allocation `i` created again, this time asking for an identity, so an
     * open has a shared object to count. Answers the record the miniport
     * wrote back, which is what the opening process is handed. */
    av::SharedObjectRecord share(std::uint32_t i)
    {
        av::DxgkargDestroyAllocationRecord destroy = {};
        void *const list[] = {infos_[i].hAllocation};
        destroy.NumAllocations = 1u;
        destroy.pAllocationList = const_cast<void **>(list);
        CHECK_STATUS(allocations_.destroy(destroy), Status::ok);

        const AllocationDescriptor shadow(AllocationKind::shadow, true, SurfaceLayout(10u, 10u, 21u, 40u),
                                          400u, ScanoutTarget());
        CHECK_STATUS(shadow.encode(descriptors_[i], AllocationDescriptor::encoded_size), Status::ok);
        const av::SharedObjectRecord asking(0u, av::SharedObjectKind(av::SharedObjectKind::memory), 0u,
                                            0x1000u);
        CHECK_STATUS(asking.encode(descriptors_[i] + AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        std::memset(&infos_[i], 0, sizeof infos_[i]);
        infos_[i].pPrivateDriverData = descriptors_[i];
        infos_[i].PrivateDriverDataSize =
            AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size;
        DxgkargCreateAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationInfo = &infos_[i];
        CHECK_STATUS(allocations_.create(request), Status::ok);
        kernel_handles_.remove(handles_[i]);
        handles_[i] = kernel_handles_.insert(infos_[i].hAllocation);

        av::SharedObjectRecord record;
        CHECK_STATUS(record.decode(descriptors_[i] + AllocationDescriptor::encoded_size,
                                   av::SharedObjectRecord::encoded_size),
                     Status::ok);
        return record;
    }

    /* Allocation `i` created again as a context's ring, which is published to
     * the host as a blob over the pages the aperture gives it - so it belongs
     * to the one device that opened it and a second device may not have it. */
    void make_blob(std::uint32_t i)
    {
        av::DxgkargDestroyAllocationRecord destroy = {};
        void *const list[] = {infos_[i].hAllocation};
        destroy.NumAllocations = 1u;
        destroy.pAllocationList = const_cast<void **>(list);
        CHECK_STATUS(allocations_.destroy(destroy), Status::ok);
        const AllocationDescriptor ring(AllocationKind::context_ring, true, SurfaceLayout(), 0x1000u,
                                        ScanoutTarget());
        CHECK_STATUS(ring.encode(descriptors_[i], AllocationDescriptor::encoded_size), Status::ok);
        std::memset(&infos_[i], 0, sizeof infos_[i]);
        infos_[i].pPrivateDriverData = descriptors_[i];
        infos_[i].PrivateDriverDataSize = AllocationDescriptor::encoded_size;
        DxgkargCreateAllocationRecord request = {};
        request.NumAllocations = 1u;
        request.pAllocationInfo = &infos_[i];
        CHECK_STATUS(allocations_.create(request), Status::ok);
        kernel_handles_.remove(handles_[i]);
        handles_[i] = kernel_handles_.insert(infos_[i].hAllocation);
    }

    /* What dxgkrnl hands the opening process: the creator's own private data. */
    void hand_over_creator_data(std::uint32_t i)
    {
        opens_[i].pPrivateDriverData = descriptors_[i];
        opens_[i].PrivateDriverDataSize =
            AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size;
    }

    av::SharedObjectTable &shared() { return shared_; }

    /* The creator's own record bytes, which a test may rewrite as a hostile
     * process would. */
    std::uint8_t *creator_record(std::uint32_t i)
    { return descriptors_[i] + AllocationDescriptor::encoded_size; }

    MockPlatform                 &platform() { return platform_; }
    Adapter                      &adapter() { return adapter_; }
    AllocationTable              &allocations() { return allocations_; }
    DeviceTable                  &devices() { return devices_; }
    DxgkOpenAllocationInfoRecord &open_info(std::uint32_t i) { return opens_[i]; }
    FakeHandleTable              &kernel_handles() { return kernel_handles_; }
    std::uint32_t                 handle(std::uint32_t i) const { return handles_[i]; }
    const void                   *allocation_handle(std::uint32_t i) const { return infos_[i].hAllocation; }
    const void                   *runtime_device() const { return &runtime_device_; }

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
    av::HostVisibleSegment        host_visible_;
    av::MemorySegments            segments_;
    av::SharedObjectTable shared_;
    AllocationTable              allocations_;
    DeviceTable                  devices_;
    DxgkAllocationInfoRecord     infos_[allocation_count];
    /* Room for the record a shareable allocation carries after its
     * descriptor (feature 053). */
    std::uint8_t                 descriptors_[allocation_count][AllocationDescriptor::encoded_size +
                                                                av::SharedObjectRecord::encoded_size];
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

/* ---- report -------------------------------------------------------------------- */

namespace {

unsigned long long hex_of(const void *object) { return static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(object)); }

}  // namespace

TEST(device_table_report, writes_each_device_and_the_allocations_it_has_open)
{ Fixture fixture;
  void *device = fixture.create_device();
  void *other = fixture.create_device();
  DxgkargOpenAllocationRecord request = fixture.open_request(2u);
  CHECK_STATUS(fixture.devices().open(device, request), Status::ok);
  DxgkargOpenAllocationRecord other_request = fixture.open_request(1u);
  CHECK_STATUS(fixture.devices().open(other, other_request), Status::ok);
  char bytes[1024];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.devices().report(writer);
  char expected[1024];
  std::snprintf(expected, sizeof expected,
                "device_table/0 busy=false devices=2 opens=3\n"
                "device_table/0/device/0 handle=0x%016llX runtime=0x%016llX flags=0x0000000000000000\n"
                "device_table/0/device/0/open/0 allocation=0x%016llX\n"
                "device_table/0/device/0/open/1 allocation=0x%016llX\n"
                "device_table/0/device/1 handle=0x%016llX runtime=0x%016llX flags=0x0000000000000000\n"
                "device_table/0/device/1/open/0 allocation=0x%016llX",
                hex_of(device), hex_of(fixture.runtime_device()), hex_of(fixture.allocation_handle(0u)),
                hex_of(fixture.allocation_handle(1u)), hex_of(other), hex_of(fixture.runtime_device()),
                hex_of(fixture.allocation_handle(0u)));
  CHECK(std::strcmp(writer.dump(), expected) == 0);
  CHECK(fixture.devices().invariants_hold());
  CHECK_EQ(fixture.platform().locks_held(), 0u); }

TEST(device_table_report, a_table_another_thread_holds_is_only_busy)
{ Fixture fixture;
  fixture.create_device();
  fixture.platform().hold_elsewhere(av::LockRank::device_table);
  char bytes[128];
  av::StateWriter writer(bytes, sizeof bytes);
  fixture.devices().report(writer);
  CHECK(std::strcmp(writer.dump(), "device_table/0 busy=true") == 0);
  fixture.platform().set_irql(Irql::dispatch);
  fixture.devices().report(writer);
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  CHECK(fixture.platform().claim(Violation::irql_ceiling));
  fixture.platform().set_irql(Irql::passive); }

/* ---- shared allocations (feature 053) ------------------------------------------ */

/* The second process's open is a second device's, with the creating process's
 * private data in front of it: the identity is what joins them, and the count
 * on the object is what keeps the host resource alive. */
TEST(device_table_open, a_second_device_opening_a_shared_allocation_counts_the_object)
{ Fixture fixture;
  const av::SharedObjectRecord record = fixture.share(0u);
  CHECK(record.assigned());
  void *const first = fixture.create_device();
  int         second_runtime = 0;
  DxgkargCreateDeviceRecord make_second = {};
  make_second.hDevice = &second_runtime;
  CHECK_STATUS(fixture.devices().create(fixture.adapter(), make_second), Status::ok);
  void *const second = make_second.hDevice;

  DxgkargOpenAllocationRecord open = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(first, open), Status::ok);
  CHECK(fixture.devices().invariants_hold());
  CHECK(fixture.shared().invariants_hold());
  void *const held_by_first = fixture.open_info(0u).hDeviceSpecificAllocation;

  DxgkargOpenAllocationRecord again = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(second, again), Status::ok);
  CHECK(fixture.devices().invariants_hold());
  CHECK_EQ(fixture.allocation(0u)->opens(), 2u);
  CHECK_EQ(fixture.shared().live(), 1u);

  /* The first process exits holding it; the second keeps what it has. */
  CHECK_STATUS(fixture.devices().destroy(first), Status::ok);
  CHECK(fixture.shared().invariants_hold());
  CHECK_EQ(fixture.allocation(0u)->opens(), 1u);
  CHECK_EQ(fixture.shared().live(), 1u);
  static_cast<void>(held_by_first);

  void *list[] = {fixture.open_info(0u).hDeviceSpecificAllocation};
  DxgkargCloseAllocationRecord close = {};
  close.NumAllocations = 1u;
  close.pOpenHandleList = list;
  CHECK_STATUS(fixture.devices().close(second, close), Status::ok);
  CHECK_EQ(fixture.allocation(0u)->opens(), 0u);
  CHECK(fixture.shared().invariants_hold()); }

TEST(device_table_open, refuses_private_data_that_does_not_carry_the_creators_record)
{ const std::uint32_t lengths[] = {0u, AllocationDescriptor::encoded_size,
                                   AllocationDescriptor::encoded_size + av::SharedObjectRecord::encoded_size - 1u};
  for (const std::uint32_t length : lengths) {
      Fixture fixture;
      static_cast<void>(fixture.share(0u));
      void *const device = fixture.create_device();
      DxgkargOpenAllocationRecord open = fixture.open_request(1u);
      fixture.hand_over_creator_data(0u);
      fixture.open_info(0u).PrivateDriverDataSize = length;
      if (length == 0u) fixture.open_info(0u).pPrivateDriverData = nullptr;
      CHECK_STATUS(fixture.devices().open(device, open), Status::invalid_parameter);
      CHECK_EQ(fixture.devices().opens(), 0u);
      CHECK_EQ(fixture.allocation(0u)->opens(), 0u);
      CHECK(fixture.devices().invariants_hold());
      CHECK(fixture.shared().invariants_hold());
  } }

/* A process that writes its own record over the creator's names an object the
 * allocation is not: neither half alone is enough to open. */
TEST(device_table_open, refuses_a_record_that_names_another_object_than_the_allocation)
{ Fixture fixture;
  const av::SharedObjectRecord record = fixture.share(0u);
  void *const device = fixture.create_device();
  const av::SharedObjectRecord forged(record.object_id() + 1u, record.kind(), record.adapter_id(), 0x1000u);
  CHECK_STATUS(forged.encode(fixture.creator_record(0u), av::SharedObjectRecord::encoded_size), Status::ok);
  DxgkargOpenAllocationRecord open = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(device, open), Status::invalid_parameter);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK(fixture.shared().invariants_hold()); }

/* A record from another adapter, which is the multi-GPU edge case: the id may
 * even be one this adapter minted, and it is still refused. */
TEST(device_table_open, refuses_a_record_whose_adapter_is_not_this_one)
{ Fixture fixture;
  const av::SharedObjectRecord record = fixture.share(0u);
  void *const device = fixture.create_device();
  const av::SharedObjectRecord elsewhere(record.object_id(), record.kind(), record.adapter_id() ^ 0x40u,
                                         0x1000u);
  CHECK_STATUS(elsewhere.encode(fixture.creator_record(0u), av::SharedObjectRecord::encoded_size),
               Status::ok);
  DxgkargOpenAllocationRecord open = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(device, open), Status::not_found);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK(fixture.shared().invariants_hold()); }

TEST(device_table_open, refuses_a_record_that_does_not_decode_at_all)
{ Fixture fixture;
  static_cast<void>(fixture.share(0u));
  void *const device = fixture.create_device();
  fixture.creator_record(0u)[0] ^= 0xFFu;   /* the magic */
  DxgkargOpenAllocationRecord open = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(device, open), Status::invalid_parameter);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK(fixture.shared().invariants_hold()); }

/* An allocation with no identity is opened as it always was, and its open
 * carries no object to uncount. */
TEST(device_table_open, an_allocation_with_no_identity_needs_no_record_at_all)
{ Fixture fixture;
  void *const device = fixture.create_device();
  DxgkargOpenAllocationRecord open = fixture.open_request(1u);
  CHECK_STATUS(fixture.devices().open(device, open), Status::ok);
  CHECK_EQ(fixture.shared().live(), 0u);
  CHECK(fixture.devices().invariants_hold());
  CHECK_STATUS(fixture.close(device, 1u), Status::ok);
  CHECK(fixture.shared().invariants_hold()); }

/* An open that claims the object and then cannot record it must give the
 * reference straight back: a count that only ever went up would make the
 * host descriptor immortal. */
TEST(device_table_open, gives_the_shared_reference_back_when_the_open_itself_cannot_be_recorded)
{ Fixture fixture;
  static_cast<void>(fixture.share(0u));
  void *const device = fixture.create_device();
  DxgkargOpenAllocationRecord open = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  fixture.platform().fail_allocation_at(1u);
  CHECK_STATUS(fixture.devices().open(device, open), Status::no_memory);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK_EQ(fixture.allocation(0u)->opens(), 0u);
  CHECK(fixture.shared().invariants_hold());
  /* Nothing is held, so the next open still works. */
  DxgkargOpenAllocationRecord again = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(device, again), Status::ok);
  CHECK_STATUS(fixture.close(device, 1u), Status::ok);
  CHECK(fixture.shared().invariants_hold()); }

/* A list is all-or-nothing, so an earlier shared open is unwound when a later
 * one in the same call fails. */
TEST(device_table_open, unwinds_an_earlier_shared_open_when_a_later_one_fails)
{ Fixture fixture;
  static_cast<void>(fixture.share(0u));
  void *const device = fixture.create_device();
  DxgkargOpenAllocationRecord open = fixture.open_request(2u);
  fixture.hand_over_creator_data(0u);
  /* The second names an allocation with no identity, and its private data is
   * the one thing this open cannot resolve. */
  fixture.open_info(1u).hAllocation = 0u;
  CHECK_STATUS(fixture.devices().open(device, open), Status::invalid_parameter);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK_EQ(fixture.allocation(0u)->opens(), 0u);
  CHECK(fixture.devices().invariants_hold());
  CHECK(fixture.shared().invariants_hold());
  /* And the object is still openable, which it would not be if the unwind had
   * dropped a reference it never took. */
  DxgkargOpenAllocationRecord again = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(device, again), Status::ok);
  CHECK_STATUS(fixture.close(device, 1u), Status::ok); }

/* A shared allocation opened first and a later one in the same list refused:
 * the unwind has to give the *object* back too, not only the open, or the
 * identity is held by a device that opened nothing (feature 053). */
TEST(device_table_open, unwinds_the_object_of_a_shared_open_when_a_later_open_cannot_be_recorded)
{ Fixture fixture;
  static_cast<void>(fixture.share(0u));
  void *const device = fixture.create_device();
  DxgkargOpenAllocationRecord request = fixture.open_request(2u);
  static_cast<void>(fixture.share(1u));
  fixture.hand_over_creator_data(0u);
  fixture.hand_over_creator_data(1u);
  /* The second names an identity whose record does not decode, which is
   * refused inside the loop rather than before it - so the first is already
   * open, and shared, when the unwind runs. */
  for (std::uint32_t index = 0u; index < av::SharedObjectRecord::encoded_size; index++)
      fixture.creator_record(1u)[index] = 0xA5u;
  CHECK(fixture.devices().open(device, request) != Status::ok);
  CHECK_EQ(fixture.devices().opens(), 0u);
  CHECK_EQ(fixture.allocation(0u)->opens(), 0u);
  CHECK(fixture.devices().invariants_hold());
  CHECK(fixture.shared().invariants_hold());
  /* And the object is openable again, which it would not be had the unwind
   * kept the reference the first open took. */
  DxgkargOpenAllocationRecord again = fixture.open_request(1u);
  fixture.hand_over_creator_data(0u);
  CHECK_STATUS(fixture.devices().open(device, again), Status::ok);
  CHECK_STATUS(fixture.close(device, 1u), Status::ok); }

/* A context's ring belongs to the device that opened it: the host reads it over
 * the pages that device's aperture mapping gave, so a second device opening the
 * same blob would be reading another context's ring. The open is refused, and
 * the refusal has to unwind what it had already recorded. */
TEST(device_table_open, a_blob_another_device_already_holds_is_refused_and_unwound)
{ Fixture fixture;
  fixture.make_blob(0u);
  void *const first = fixture.create_device();
  DxgkargOpenAllocationRecord mine = fixture.open_request(1u);
  CHECK_STATUS(fixture.devices().open(first, mine), Status::ok);
  CHECK_EQ(fixture.allocation(0u)->opens(), 1u);

  void *const other = fixture.create_device();
  DxgkargOpenAllocationRecord theirs = fixture.open_request(1u);
  CHECK_STATUS(fixture.devices().open(other, theirs), Status::invalid_parameter);
  CHECK(theirs.pOpenAllocation[0].hDeviceSpecificAllocation == nullptr);
  /* The first device still has it, and exactly once. */
  CHECK_EQ(fixture.allocation(0u)->opens(), 1u);
  CHECK_EQ(fixture.devices().opens(), 1u);
  CHECK(fixture.devices().invariants_hold());
  CHECK(fixture.shared().invariants_hold()); }
