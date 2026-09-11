#include "core/ddi/dxgk_present_allocation_info_record.hpp"
#include "core/ddi/dxgkarg_present_record.hpp"
#include "core/present/virtual_present_addresses.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>
#include <cstring>

using av::DxgkargPresentRecord;
using av::DxgkPresentAllocationInfoRecord;
using av::Irql;
using av::MockPlatform;
using av::PresentAddressSpace;
using av::Status;
using av::VirtualPresentAddresses;
using av::Violation;

TEST(virtual_present_addresses, each_reference_is_the_allocation_s_gpu_virtual_address)
{ MockPlatform platform;
  DxgkPresentAllocationInfoRecord allocations[3] = {};
  allocations[1].AllocationVirtualAddress = 0x0102030405060708ull;
  DxgkargPresentRecord request = {};
  request.pAllocationList = allocations;
  VirtualPresentAddresses addresses(platform, request);
  CHECK(addresses.space() == PresentAddressSpace::gpu_virtual);
  CHECK_STATUS(addresses.validate(2u), Status::ok);
  std::uint8_t command[72];
  std::memset(command, 0xEE, sizeof command);
  addresses.refer(command, 1u, 48u);
  CHECK_EQ(command[48], 0x08u);
  CHECK_EQ(command[55], 0x01u);
  CHECK_EQ(command[47], 0xEEu);
  CHECK_EQ(command[56], 0xEEu);
  request.pAllocationList = nullptr;
  CHECK_STATUS(addresses.validate(1u), Status::invalid_parameter); }

TEST(virtual_present_addresses, is_passive_only)
{ MockPlatform platform; const DxgkargPresentRecord request = {};
  const VirtualPresentAddresses addresses(platform, request);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(addresses.space());
  static_cast<void>(addresses.validate(1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }
