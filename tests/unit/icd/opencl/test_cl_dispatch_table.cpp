/* The table the Khronos loader dispatches through, and what is in the slots
 * this ICD does not claim.
 *
 * The ICD ABI is positional and unchecked: the loader reads a slot and jumps,
 * so a null one is a jump into nothing with no diagnostic. What is asserted
 * here is that no slot a carried command could reach is null, that the slots
 * this ICD claims hold its own entries, and that the rest refuse rather than
 * pretend - an application asking for something this driver does not implement
 * is told so, and the process survives being asked.
 *
 * `ClIcdBinding` is what those entries reach the root through, and `ClIcdRoot`
 * is what binds itself to it. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_icd_binding.hpp"

#include <CL/cl_ext.h>

using av::ClIcdRig;
using av::clicd::ClDispatchTable;
using av::clicd::ClIcdBinding;
using av::clicd::ClIcdRoot;

TEST(cl_dispatch_table, a_table_that_was_never_filled_claims_nothing)
{ ClDispatchTable table;
  CHECK(!table.filled());
  CHECK_EQ(table.claimed(), 0u);
  CHECK(table.invariants_hold()); }

TEST(cl_dispatch_table, filling_claims_this_icds_entries_and_refuses_everywhere_else)
{ ClDispatchTable table;
  table.fill();
  CHECK(table.filled());
  CHECK(table.claimed() > 0u);
  CHECK(table.invariants_hold());
  const cl_icd_dispatch &slots = table.table();
  /* Claimed. */
  CHECK(slots.clGetPlatformIDs != nullptr);
  CHECK(slots.clCreateContext != nullptr);
  CHECK(slots.clEnqueueNDRangeKernel != nullptr);
  /* Not claimed, and still not null: the loader would call through it. */
  CHECK(slots.clCreateImage != nullptr);
  CHECK(slots.clSVMAlloc != nullptr);
  CHECK(slots.clCreateSubDevices != nullptr); }

TEST(cl_dispatch_table, filling_twice_changes_nothing)
{ ClDispatchTable first;
  ClDispatchTable second;
  first.fill();
  second.fill();
  second.fill();
  CHECK_EQ(first.claimed(), second.claimed());
  CHECK(first.table().clGetPlatformIDs == second.table().clGetPlatformIDs);
  CHECK(first.table().clCreateImage == second.table().clCreateImage);
  CHECK(first.invariants_hold());
  CHECK(second.invariants_hold()); }

TEST(cl_dispatch_table, the_enumeration_is_answered_outside_the_table)
{ ClDispatchTable table;
  table.fill();
  /* cl_icd_dispatch has no slot for clIcdGetPlatformIDsKHR: the loader asks
   * clGetExtensionFunctionAddress for it by name, before it has a platform to
   * dispatch through at all. */
  CHECK(table.icd_get_platform_ids() != nullptr); }

TEST(cl_dispatch_table, an_unclaimed_slot_refuses_and_the_process_survives_it)
{ ClDispatchTable table;
  table.fill();
  const cl_icd_dispatch &slots = table.table();
  CHECK_EQ(slots.clCreateSubDevices(nullptr, nullptr, 0u, nullptr, nullptr), CL_INVALID_OPERATION);
  cl_int error = CL_SUCCESS;
  CHECK(slots.clSVMAlloc(nullptr, 0u, 16u, 0u) == nullptr);
  CHECK(slots.clCreateImage(nullptr, 0u, nullptr, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION); }

TEST(cl_icd_binding, a_root_binds_itself_and_unbinds_when_it_goes)
{ ClIcdBinding binding;
  CHECK(!binding.bound());
  { ClIcdRig rig;
    /* The rig's root binds to the process's binding, not this one - which is
     * the point of having one: an entry point handed no context reaches the
     * root through exactly one pointer. */
    CHECK(av::clicd::cl_icd_binding.bound());
    CHECK(&av::clicd::cl_icd_binding.root() == &rig.root()); }
  CHECK(!av::clicd::cl_icd_binding.bound());
  CHECK(!binding.bound()); }

TEST(cl_icd_binding, an_entry_reached_before_a_root_exists_refuses)
{ /* No rig, so nothing is bound: what an application sees when the driver
   * failed to start. The loader still calls through the table. */
  CHECK(!av::clicd::cl_icd_binding.bound());
  ClDispatchTable table;
  table.fill();
  cl_uint count = 7u;
  CHECK_EQ(table.table().clGetPlatformIDs(0u, nullptr, &count), CL_PLATFORM_NOT_FOUND_KHR);
  CHECK_EQ(table.table().clGetPlatformInfo(nullptr, CL_PLATFORM_NAME, 0u, nullptr, nullptr),
           CL_INVALID_PLATFORM);
  CHECK_EQ(table.table().clReleaseContext(nullptr), CL_INVALID_CONTEXT); }
