#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "miniport/present/patched_present_addresses.hpp"
#include "miniport/present/present_addressing.hpp"
#include "miniport/present/virtual_present_addresses.hpp"
#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "harness/platform/violation.hpp"

#include <cstdint>

using av::D3dddiPatchLocationListRecord;
using av::DxgkargPresentRecord;
using av::Irql;
using av::MockPlatform;
using av::PatchedPresentAddresses;
using av::PresentAddressSpace;
using av::Status;
using av::Violation;

TEST(patched_present_addresses, each_reference_is_a_patch_location)
{ MockPlatform platform;
  D3dddiPatchLocationListRecord patches[2] = {};
  DxgkargPresentRecord request = {};
  request.pPatchLocationListOut = patches;
  request.PatchLocationListOutSize = 2u;
  PatchedPresentAddresses addresses(platform, request);
  CHECK(addresses.space() == PresentAddressSpace::physical);
  CHECK_STATUS(addresses.validate(2u), Status::ok);
  CHECK_STATUS(addresses.validate(3u), Status::invalid_parameter);
  std::uint8_t command[72] = {};
  addresses.refer(command, 2u, 56u);
  CHECK_EQ(patches[0].AllocationIndex, 2u);
  CHECK_EQ(patches[0].PatchOffset, 56u);
  CHECK(request.pPatchLocationListOut == patches + 1);
  CHECK_EQ(command[56], 0u);
  request.pPatchLocationListOut = nullptr;
  CHECK_STATUS(addresses.validate(1u), Status::invalid_parameter); }

/* PresentRequest holds one or the other through the interface. */
TEST(present_addressing, patched_and_virtual_answer_their_address_space)
{ MockPlatform platform; DxgkargPresentRecord request = {};
  PatchedPresentAddresses patched(platform, request);
  av::VirtualPresentAddresses virtual_addresses(platform, request);
  const av::PresentAddressing &physical = patched;
  const av::PresentAddressing &gpu_virtual = virtual_addresses;
  CHECK(physical.space() == PresentAddressSpace::physical);
  CHECK(gpu_virtual.space() == PresentAddressSpace::gpu_virtual); }

TEST(patched_present_addresses, is_passive_only)
{ MockPlatform platform; DxgkargPresentRecord request = {};
  const PatchedPresentAddresses addresses(platform, request);
  platform.set_irql(Irql::dispatch);
  static_cast<void>(addresses.space());
  static_cast<void>(addresses.validate(1u));
  platform.set_irql(Irql::passive);
  CHECK(platform.claim(Violation::irql_ceiling));
  while (platform.claim(Violation::irql_ceiling)) {} }
