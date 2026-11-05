#include "miniport/adapter/adapter.hpp"
#include "miniport/memory/allocation.hpp"
#include "miniport/memory/aperture_segment.hpp"
#include "miniport/memory/memory_segments.hpp"
#include "miniport/memory/point_view.hpp"
#include "miniport/memory/device_allocation.hpp"
#include "miniport/memory/shared_object.hpp"
#include "miniport/scheduling/device.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"

using av::Adapter;
using av::Allocation;
using av::AllocationDescriptor;
using av::Device;
using av::DeviceAllocation;
using av::MockPlatform;

TEST(device_allocation, joins_one_device_to_one_allocation_and_names_no_shared_object)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u, nullptr);
  av::ApertureSegment aperture(platform);
  const av::MemorySegments segments(platform, aperture);
  Allocation allocation(platform, segments, AllocationDescriptor(), 0x1000u);
  const DeviceAllocation open(device, allocation, nullptr);
  CHECK(&open.device() == &device);
  CHECK(&open.allocation() == &allocation);
  CHECK(open.shared() == nullptr); }

TEST(device_allocation, carries_the_shared_object_its_open_counted)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u, nullptr);
  av::ApertureSegment aperture(platform);
  const av::MemorySegments segments(platform, aperture);
  Allocation allocation(platform, segments, AllocationDescriptor(), 0x1000u);
  av::SharedObject object(platform, 7u, av::SharedObjectKind(av::SharedObjectKind::memory), 0x1000u,
                          allocation);
  const DeviceAllocation open(device, allocation, &object);
  CHECK(open.shared() == &object); }

TEST(device_allocation, keeps_the_point_named_for_its_next_present_until_forgotten)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u, nullptr);
  av::ApertureSegment aperture(platform);
  const av::MemorySegments segments(platform, aperture);
  Allocation allocation(platform, segments, AllocationDescriptor(), 0x1000u);
  DeviceAllocation open(device, allocation, nullptr);
  CHECK_EQ(open.point(), 0u);
  open.name_point(7u);
  CHECK_EQ(open.point(), 7u);
  open.name_point(8u);
  CHECK_EQ(open.point(), 8u);
  open.forget_point();
  CHECK_EQ(open.point(), 0u); }

TEST(device_allocation, withdraws_a_point_named_before_and_nothing_else)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u, nullptr);
  av::ApertureSegment aperture(platform);
  const av::MemorySegments segments(platform, aperture);
  Allocation allocation(platform, segments, AllocationDescriptor(), 0x1000u);
  DeviceAllocation open(device, allocation, nullptr);
  open.name_point(7u);
  open.name_point(8u);
  CHECK_EQ(open.queued(), 2u);
  CHECK(!open.withdraw_point(9u));
  CHECK_EQ(open.queued(), 2u);
  CHECK(open.withdraw_point(7u));
  CHECK_EQ(open.queued(), 1u);
  CHECK_EQ(open.point(), 8u);
  CHECK(open.withdraw_point(8u));
  CHECK_EQ(open.queued(), 0u);
  CHECK_EQ(open.point(), 0u);
  CHECK(!open.withdraw_point(8u)); }

TEST(device_allocation, is_pointed_once_a_point_is_named_and_tells_the_first_starvation_once)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u, nullptr);
  av::ApertureSegment aperture(platform);
  const av::MemorySegments segments(platform, aperture);
  Allocation allocation(platform, segments, AllocationDescriptor(), 0x1000u);
  DeviceAllocation open(device, allocation, nullptr);
  av::PointView view;
  open.view(view);
  CHECK(!open.pointed());
  CHECK(!view.pointed());
  CHECK(!view.queued());
  CHECK(!view.first_starvation());
  open.name_point(5u);
  CHECK(open.pointed());
  open.view(view);
  CHECK(view.pointed());
  CHECK(view.queued());
  CHECK_EQ(view.point(), 5u);
  CHECK(!view.first_starvation());
  open.forget_point();
  CHECK(open.pointed());
  open.view(view);
  CHECK(view.pointed());
  CHECK(!view.queued());
  CHECK_EQ(view.point(), 0u);
  CHECK(view.first_starvation());
  open.view(view);
  CHECK(!view.first_starvation()); }
