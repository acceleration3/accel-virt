#include "core/adapter/adapter.hpp"
#include "core/memory/allocation.hpp"
#include "core/memory/device_allocation.hpp"
#include "core/scheduling/device.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "proto/resources/allocation_descriptor.hpp"

using av::Adapter;
using av::Allocation;
using av::AllocationDescriptor;
using av::Device;
using av::DeviceAllocation;
using av::MockPlatform;

TEST(device_allocation, joins_one_device_to_one_allocation)
{ MockPlatform platform; int pdo = 0;
  Adapter adapter(platform, &pdo);
  Device device(platform, adapter, nullptr, 0u);
  Allocation allocation(platform, AllocationDescriptor(), 0x1000u);
  const DeviceAllocation open(device, allocation);
  CHECK(&open.device() == &device);
  CHECK(&open.allocation() == &allocation); }
