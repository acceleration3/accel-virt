#include "../../src/core/adapter.hpp"
#include "../../src/core/allocation.hpp"
#include "../../src/core/device.hpp"
#include "../../src/core/device_allocation.hpp"
#include "../../src/proto/allocation_descriptor.hpp"
#include "../harness/mock_platform.hpp"
#include "../harness/test_registry.hpp"

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
