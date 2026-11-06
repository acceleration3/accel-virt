#include "miniport/ddi/d3dddi_patch_location_list_record.hpp"
#include "miniport/ddi/dxgk_allocation_list_record.hpp"
#include "miniport/ddi/dxgk_present_allocation_info_record.hpp"
#include "miniport/ddi/dxgkarg_present_record.hpp"
#include "miniport/present/presented_allocations.hpp"
#include "harness/framework/test_registry.hpp"

using av::D3dddiPatchLocationListRecord;
using av::DxgkAllocationListRecord;
using av::DxgkargPresentRecord;
using av::DxgkPresentAllocationInfoRecord;
using av::PresentedAllocations;

namespace {

int first, second, third;

}

TEST(presented_allocations, a_present_with_no_list_names_nothing)
{ DxgkargPresentRecord present = {};
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const PresentedAllocations named(present);
  CHECK(named.source() == nullptr);
  CHECK(named.destination() == nullptr); }

TEST(presented_allocations, a_counted_blt_and_fill_name_the_source_then_the_destination)
{ D3dddiPatchLocationListRecord patches[2] = {};
  DxgkAllocationListRecord      list[2] = {};
  list[0].hDeviceSpecificAllocation = &first;
  list[1].hDeviceSpecificAllocation = &second;
  DxgkargPresentRecord present = {};
  present.pAllocationList = list;
  present.pPatchLocationListOut = patches;
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const PresentedAllocations blt(present);
  CHECK(blt.source() == &first);
  CHECK(blt.destination() == &second);
  present.NumSrcAllocations = 0u;
  const PresentedAllocations fill(present);
  CHECK(fill.source() == nullptr);
  CHECK(fill.destination() == &first); }

TEST(presented_allocations, counts_it_cannot_read_name_nothing)
{ D3dddiPatchLocationListRecord patches[2] = {};
  DxgkAllocationListRecord      list[3] = {};
  list[0].hDeviceSpecificAllocation = &first;
  list[1].hDeviceSpecificAllocation = &second;
  list[2].hDeviceSpecificAllocation = &third;
  DxgkargPresentRecord present = {};
  present.pAllocationList = list;
  present.pPatchLocationListOut = patches;
  present.NumSrcAllocations = 2u;
  present.NumDstAllocations = 1u;
  const PresentedAllocations two_sources(present);
  CHECK(two_sources.source() == nullptr);
  CHECK(two_sources.destination() == nullptr);
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 2u;
  const PresentedAllocations two_destinations(present);
  CHECK(two_destinations.source() == nullptr);
  CHECK(two_destinations.destination() == nullptr);
  present.NumSrcAllocations = 0u;
  present.NumDstAllocations = 0u;
  const PresentedAllocations uncounted(present);
  CHECK(uncounted.source() == nullptr);
  CHECK(uncounted.destination() == nullptr); }

TEST(presented_allocations, an_uncounted_virtual_present_names_the_fixed_indices)
{ DxgkPresentAllocationInfoRecord info[3] = {};
  info[0].hDeviceSpecificAllocation = &third;
  info[1].hDeviceSpecificAllocation = &first;
  info[2].hDeviceSpecificAllocation = &second;
  static_assert(sizeof(DxgkPresentAllocationInfoRecord) != sizeof(DxgkAllocationListRecord),
                "the strides differ, which this test depends on");
  DxgkargPresentRecord present = {};
  present.pAllocationList = info;
  const PresentedAllocations named(present);
  CHECK(named.source() == &first);
  CHECK(named.destination() == &second);
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const PresentedAllocations counted(present);
  CHECK(counted.source() == &third);
  CHECK(counted.destination() == &first);
  present.NumSrcAllocations = 0u;
  const PresentedAllocations fill(present);
  CHECK(fill.source() == nullptr);
  CHECK(fill.destination() == &third); }
