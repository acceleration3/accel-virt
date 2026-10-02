/* Which allocations a present names: dxgkrnl's own presents count them, and an
 * application's blt on a GPU virtual address context names them at the fixed
 * indices with no counts - each in the list's own stride. */
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

}  // namespace

TEST(presented_allocations, a_present_with_no_list_names_nothing)
{ DxgkargPresentRecord present = {};
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const PresentedAllocations named(present);
  CHECK(named.source() == nullptr);
  CHECK(named.destination() == nullptr); }

/* dxgkrnl's own blt and fill: the sources, then the destination. */
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

/* Two sources, or none named at all on a patched context, is not a present
 * this driver reads. */
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

/* An application's blt into its window's redirection surface: no counts, the
 * source and destination at DXGK_PRESENT_SOURCE_INDEX and _DESTINATION_INDEX,
 * read as DXGK_PRESENTALLOCATIONINFO - whose stride is not DXGK_ALLOCATIONLIST's,
 * so reading it as the other would name the wrong allocations. */
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
  /* Counted, on a virtual context: the same list, in its own stride. */
  present.NumSrcAllocations = 1u;
  present.NumDstAllocations = 1u;
  const PresentedAllocations counted(present);
  CHECK(counted.source() == &third);
  CHECK(counted.destination() == &first); }
