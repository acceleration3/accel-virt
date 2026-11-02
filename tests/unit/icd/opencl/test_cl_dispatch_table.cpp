#include "harness/framework/test_registry.hpp"
#include "harness/icd/cl_icd_rig.hpp"
#include "icd/opencl/cl_dispatch_table.hpp"
#include "icd/opencl/cl_icd_binding.hpp"

#include <CL/cl_ext.h>

using av::ClIcdRig;
using av::clicd::ClDispatchTable;
using av::clicd::ClDispatchTableProbe;

namespace av::clicd {
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

}
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
  CHECK(slots.clGetPlatformIDs != nullptr);
  CHECK(slots.clCreateContext != nullptr);
  CHECK(slots.clEnqueueNDRangeKernel != nullptr);
  CHECK(slots.clCreateBuffer != nullptr);
  CHECK(slots.clCreateBufferWithProperties != nullptr);
  CHECK(reinterpret_cast<const void *>(slots.clCreateBuffer) !=
        reinterpret_cast<const void *>(slots.clCreateBufferWithProperties));
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
  CHECK(table.icd_get_platform_ids() != nullptr); }

TEST(cl_dispatch_table, the_external_memory_pair_is_answered_outside_the_table_too)
{ ClDispatchTable table;
  table.fill();
  CHECK(table.external_mem_acquire() != nullptr);
  CHECK(table.external_mem_release() != nullptr);
  CHECK(table.external_mem_acquire() != table.external_mem_release());
  CHECK(table.external_mem_acquire() != table.icd_get_platform_ids()); }

TEST(cl_dispatch_table, the_gl_context_query_is_answered_by_name_and_by_its_slot_alike)
{ ClIcdRig        rig;
  ClDispatchTable table;
  table.fill();
  CHECK(table.gl_context_info() != nullptr);
  CHECK(table.gl_context_info() == reinterpret_cast<void *>(table.table().clGetGLContextInfoKHR));
  CHECK(rig.root().extension_function("clGetGLContextInfoKHR") == table.gl_context_info());
  CHECK(table.gl_context_info() != table.external_mem_acquire());
  CHECK(table.gl_context_info() != table.icd_get_platform_ids()); }

TEST(cl_dispatch_table, the_il_program_extension_s_create_is_the_core_slot_by_name)
{ ClIcdRig        rig;
  ClDispatchTable table;
  table.fill();
  CHECK(table.program_from_il() != nullptr);
  CHECK(table.program_from_il() == reinterpret_cast<void *>(table.table().clCreateProgramWithIL));
  CHECK(rig.root().extension_function("clCreateProgramWithILKHR") == table.program_from_il());
  CHECK(table.program_from_il() != table.gl_context_info());
  CHECK(table.table().clGetExtensionFunctionAddress("clCreateProgramWithILKHR") == table.program_from_il());
  CHECK(table.table().clGetExtensionFunctionAddress("clNoSuchThing") == nullptr); }

TEST(cl_dispatch_table, an_unclaimed_slot_refuses_and_the_process_survives_it)
{ ClDispatchTable table;
  table.fill();
  const cl_icd_dispatch &slots = table.table();
  CHECK_EQ(slots.clGetDeviceAndHostTimer(nullptr, nullptr, nullptr), CL_INVALID_OPERATION);
  CHECK(slots.clSVMAlloc(nullptr, 0u, 16u, 0u) == nullptr);
  CHECK_EQ(slots.clEnqueueNativeKernel(nullptr, nullptr, nullptr, 0u, 0u, nullptr, nullptr, 0u, nullptr, nullptr),
           CL_INVALID_OPERATION); }

TEST(cl_icd_binding, a_root_binds_itself_and_unbinds_when_it_goes)
{ ClIcdBinding binding;
  CHECK(!binding.bound());
  { ClIcdRig rig;
    CHECK(av::clicd::cl_icd_binding.bound());
    CHECK(&av::clicd::cl_icd_binding.root() == &rig.root()); }
  CHECK(!av::clicd::cl_icd_binding.bound());
  CHECK(!binding.bound()); }

TEST(cl_icd_binding, an_entry_reached_before_a_root_exists_refuses)
{
  CHECK(!av::clicd::cl_icd_binding.bound());
  ClDispatchTable table;
  table.fill();
  cl_uint count = 7u;
  CHECK_EQ(table.table().clGetPlatformIDs(0u, nullptr, &count), CL_PLATFORM_NOT_FOUND_KHR);
  CHECK_EQ(table.table().clGetPlatformInfo(nullptr, CL_PLATFORM_NAME, 0u, nullptr, nullptr),
           CL_INVALID_PLATFORM);
  CHECK_EQ(table.table().clReleaseContext(nullptr), CL_INVALID_CONTEXT);

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

  CHECK(table.table().clSVMAlloc(nullptr, 0u, 0u, 0u) == nullptr);
  table.table().clSVMFree(nullptr, nullptr);

  CHECK_EQ(table.table().clSetEventCallback(nullptr, CL_COMPLETE, nullptr, nullptr),
           CL_INVALID_EVENT);

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

  error = 7;
  CHECK(table.table().clCreateCommandQueue(nullptr, nullptr, 0u, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_CONTEXT);
  CHECK(table.table().clCreateCommandQueue(nullptr, nullptr, 0u, nullptr) == nullptr); }

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
