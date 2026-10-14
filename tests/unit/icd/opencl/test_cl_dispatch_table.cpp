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
using av::clicd::ClDispatchTableProbe;

namespace av::clicd {

/* The states the table never builds. The ICD ABI is positional and unchecked
 * - the loader reads a slot and jumps - so a filled table that claims nothing,
 * or whose own entries are not this ICD's, is a jump into nothing with no
 * diagnostic at all. */
struct ClDispatchTableProbe {
    static void filled_but_claiming_nothing(ClDispatchTable &table) { table.claimed_ = 0u; }
    static void a_slot_this_icd_does_not_own(ClDispatchTable &table)
    { table.table_.clGetPlatformIDs = nullptr; }
    static void a_platform_info_slot_of_someone_elses(ClDispatchTable &table)
    { table.table_.clGetPlatformInfo = nullptr; }
    static void a_carried_slot_left_empty(ClDispatchTable &table)
    { table.table_.clCreateImage = nullptr; }
    static void the_context_slot_left_empty(ClDispatchTable &table)
    { table.table_.clCreateContext = nullptr; }
    static void a_map_slot_of_someone_elses(ClDispatchTable &table)
    { table.table_.clEnqueueMapBuffer = nullptr; }
    static void an_unfilled_table_that_claims_something(ClDispatchTable &table)
    { table.filled_ = false; table.claimed_ = 1u; }
};

}  // namespace av::clicd
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
  /* The two buffer creates are different entries: one carries a property
   * list and the other has none to carry (feature 053). */
  CHECK(slots.clCreateBuffer != nullptr);
  CHECK(slots.clCreateBufferWithProperties != nullptr);
  CHECK(reinterpret_cast<const void *>(slots.clCreateBuffer) !=
        reinterpret_cast<const void *>(slots.clCreateBufferWithProperties));
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

/* And the other two the loader asks for by name: cl_khr_external_memory's
 * acquire and release, which the ICD ABI predates - cl_icd_dispatch has no
 * slot for either, so an application reaches them through
 * clGetExtensionFunctionAddressForPlatform or not at all. */
TEST(cl_dispatch_table, the_external_memory_pair_is_answered_outside_the_table_too)
{ ClDispatchTable table;
  table.fill();
  CHECK(table.external_mem_acquire() != nullptr);
  CHECK(table.external_mem_release() != nullptr);
  /* Two entry points, not one answered twice: an application that acquires
   * where it meant to release would otherwise never find out. */
  CHECK(table.external_mem_acquire() != table.external_mem_release());
  CHECK(table.external_mem_acquire() != table.icd_get_platform_ids()); }

/* clGetGLContextInfoKHR is asked before there is any object to dispatch
 * through, so a loader finds it by name as well as in its slot: both routes
 * reach the one entry, and the name answers nothing else. */
TEST(cl_dispatch_table, the_gl_context_query_is_answered_by_name_and_by_its_slot_alike)
{ ClIcdRig        rig;
  ClDispatchTable table;
  table.fill();
  CHECK(table.gl_context_info() != nullptr);
  CHECK(table.gl_context_info() == reinterpret_cast<void *>(table.table().clGetGLContextInfoKHR));
  CHECK(rig.root().extension_function("clGetGLContextInfoKHR") == table.gl_context_info());
  CHECK(table.gl_context_info() != table.external_mem_acquire());
  CHECK(table.gl_context_info() != table.icd_get_platform_ids()); }

TEST(cl_dispatch_table, an_unclaimed_slot_refuses_and_the_process_survives_it)
{ ClDispatchTable table;
  table.fill();
  const cl_icd_dispatch &slots = table.table();
  CHECK_EQ(slots.clCreateSubDevices(nullptr, nullptr, 0u, nullptr, nullptr), CL_INVALID_OPERATION);
  cl_int error = CL_SUCCESS;
  CHECK(slots.clSVMAlloc(nullptr, 0u, 16u, 0u) == nullptr);
  CHECK(slots.clCreateSubBuffer(nullptr, 0u, CL_BUFFER_CREATE_TYPE_REGION, nullptr, &error) == nullptr);
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
  CHECK_EQ(table.table().clReleaseContext(nullptr), CL_INVALID_CONTEXT);

  /* Every entry that answers an object rather than a status: each has its own
   * "nothing to reach" path, and each must write the API's own error into the
   * out-parameter the application gave it - a driver that failed to start
   * must not leave that word as the application had it, because the
   * application reads it to find out why. */
  cl_int error = 7;
  CHECK(table.table().clCreateContext(nullptr, 0u, nullptr, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PLATFORM);
  error = 7;
  CHECK(table.table().clCreateCommandQueueWithProperties(nullptr, nullptr, nullptr, &error) ==
        nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(table.table().clCreateBuffer(nullptr, 0u, 0u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(table.table().clCreateBufferWithProperties(nullptr, nullptr, 0u, 0u, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(table.table().clCreateProgramWithSource(nullptr, 0u, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  error = 7;
  CHECK(table.table().clCreateKernel(nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_PROGRAM);
  error = 7;
  CHECK(table.table().clEnqueueMapBuffer(nullptr, nullptr, CL_TRUE, 0u, 0u, 0u, 0u, nullptr,
                                         nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_COMMAND_QUEUE);
  error = 7;
  CHECK(table.table().clCreateUserEvent(nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);

  /* And the two that answer nothing at all: an address, and a free. Neither
   * has anywhere to report a failure, so the only right answer is to do
   * nothing and survive being called. */
  CHECK(table.table().clSVMAlloc(nullptr, 0u, 0u, 0u) == nullptr);
  table.table().clSVMFree(nullptr, nullptr);

  /* A callback registration with no root is the event that cannot be found,
   * not the argument that is missing: with no root there is no event either,
   * so the binding is what answers first. */
  CHECK_EQ(table.table().clSetEventCallback(nullptr, CL_COMPLETE, nullptr, nullptr),
           CL_INVALID_EVENT);

  /* The same calls again with nowhere to report the error. An application
   * that passes no errcode_ret is entitled to be ignored rather than to have
   * a null written through, and every one of these entries has to check. */
  CHECK(table.table().clCreateContext(nullptr, 0u, nullptr, nullptr, nullptr, nullptr) == nullptr);
  CHECK(table.table().clCreateCommandQueueWithProperties(nullptr, nullptr, nullptr, nullptr) ==
        nullptr);
  CHECK(table.table().clCreateBuffer(nullptr, 0u, 0u, nullptr, nullptr) == nullptr);
  CHECK(table.table().clCreateBufferWithProperties(nullptr, nullptr, 0u, 0u, nullptr, nullptr) ==
        nullptr);
  CHECK(table.table().clCreateProgramWithSource(nullptr, 0u, nullptr, nullptr, nullptr) ==
        nullptr);
  CHECK(table.table().clCreateKernel(nullptr, nullptr, nullptr) == nullptr);
  CHECK(table.table().clEnqueueMapBuffer(nullptr, nullptr, CL_TRUE, 0u, 0u, 0u, 0u, nullptr,
                                         nullptr, nullptr) == nullptr);
  CHECK(table.table().clCreateUserEvent(nullptr, nullptr) == nullptr);

  /* The buffer rectangles name a queue first, so that is what they refuse. */
  const size_t corner[3] = {0u, 0u, 0u}, region[3] = {1u, 1u, 1u};
  unsigned char memory[1] = {};
  CHECK_EQ(table.table().clEnqueueReadBufferRect(nullptr, nullptr, CL_TRUE, corner, corner, region, 0u, 0u, 0u, 0u,
                                                 memory, 0u, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(table.table().clEnqueueWriteBufferRect(nullptr, nullptr, CL_TRUE, corner, corner, region, 0u, 0u, 0u, 0u,
                                                  memory, 0u, nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);
  CHECK_EQ(table.table().clEnqueueCopyBufferRect(nullptr, nullptr, nullptr, corner, corner, region, 0u, 0u, 0u, 0u, 0u,
                                                 nullptr, nullptr),
           CL_INVALID_COMMAND_QUEUE);

  /* The deprecated 1.2 queue call is its own slot: an application built
   * against the older header reaches it and not the properties one, so it
   * needs the same answer. */
  error = 7;
  CHECK(table.table().clCreateCommandQueue(nullptr, nullptr, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(table.table().clCreateCommandQueue(nullptr, nullptr, 0u, nullptr) == nullptr); }

/* The invariant catches the states the table never builds - every one of
 * which the loader would turn into a jump through a null pointer. */
TEST(cl_dispatch_table, the_invariant_catches_a_table_the_loader_would_jump_into_nothing_through)
{ const auto broken = [](void (*corrupt)(ClDispatchTable &)) {
      ClDispatchTable table;
      table.fill();
      CHECK(table.invariants_hold());
      corrupt(table);
      return table.invariants_hold();
  };
  CHECK(!broken(ClDispatchTableProbe::filled_but_claiming_nothing));
  CHECK(!broken(ClDispatchTableProbe::a_slot_this_icd_does_not_own));
  CHECK(!broken(ClDispatchTableProbe::a_platform_info_slot_of_someone_elses));
  CHECK(!broken(ClDispatchTableProbe::a_carried_slot_left_empty));
  CHECK(!broken(ClDispatchTableProbe::a_map_slot_of_someone_elses));
  CHECK(!broken(ClDispatchTableProbe::the_context_slot_left_empty));

  ClDispatchTable never_filled;
  CHECK(never_filled.invariants_hold());
  ClDispatchTableProbe::an_unfilled_table_that_claims_something(never_filled);
  CHECK(!never_filled.invariants_hold()); }
