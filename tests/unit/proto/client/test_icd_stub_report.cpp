#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_icd_stubs.h"
#include "proto/client/icd_stub_report.hpp"

#include <cstring>

namespace {
struct Refusals {
    static constexpr unsigned capacity = 8u;

    unsigned    count = 0u;
    const char *api[capacity] = {};
    const char *entry[capacity] = {};
    const char *why[capacity] = {};

    void clear() { count = 0u; }

    bool named(const char *wanted) const
    {
        for (unsigned index = 0u; index < count && index < capacity; ++index) {
            if (std::strcmp(entry[index], wanted) == 0) return true;
        }
        return false;
    }
};

Refusals refusals;

cl_int CL_API_CALL claimed_get_platform_ids(cl_uint, cl_platform_id *, cl_uint *)
{
    return CL_PLATFORM_NOT_FOUND_KHR;
}

}

namespace av::icd {

void stub_called(const char *api, const char *entry_point, const char *why) noexcept
{
    if (refusals.count < Refusals::capacity) {
        refusals.api[refusals.count] = api;
        refusals.entry[refusals.count] = entry_point;
        refusals.why[refusals.count] = why;
    }
    ++refusals.count;
}

}

TEST(icd_stub_report, a_slot_the_driver_claimed_is_not_overwritten)
{ cl_icd_dispatch table{};
  table.clGetPlatformIDs = &claimed_get_platform_ids;
  av::clproto::fill_unimplemented(table);
  CHECK(table.clGetPlatformIDs == &claimed_get_platform_ids);
  refusals.clear();
  CHECK_EQ(table.clGetPlatformIDs(0u, nullptr, nullptr), CL_PLATFORM_NOT_FOUND_KHR);
  CHECK_EQ(refusals.count, 0u); }

TEST(icd_stub_report, an_unclaimed_slot_refuses_and_names_itself_once)
{ cl_icd_dispatch table{};
  av::clproto::fill_unimplemented(table);
  refusals.clear();
  CHECK(table.clGetPlatformIDs != nullptr);
  CHECK_EQ(table.clGetPlatformIDs(0u, nullptr, nullptr), CL_INVALID_OPERATION);
  CHECK_EQ(refusals.count, 1u);
  CHECK(refusals.named("clGetPlatformIDs"));
  CHECK_EQ(std::strcmp(refusals.api[0], "cl"), 0); }

TEST(icd_stub_report, a_refusal_that_returns_an_object_answers_null_and_errcode_ret)
{ cl_icd_dispatch table{};
  av::clproto::fill_unimplemented(table);
  refusals.clear();
  cl_int error = CL_SUCCESS;
  CHECK(table.clCreateContext(nullptr, 0u, nullptr, nullptr, nullptr, &error) == nullptr);
  CHECK_EQ(error, CL_INVALID_OPERATION);
  CHECK(refusals.named("clCreateContext"));
  CHECK(table.clCreateContext(nullptr, 0u, nullptr, nullptr, nullptr, nullptr) == nullptr); }

TEST(icd_stub_report, every_core_slot_the_loader_dispatches_through_is_filled)
{ cl_icd_dispatch table{};
  av::clproto::fill_unimplemented(table);
  CHECK(table.clGetPlatformInfo != nullptr);
  CHECK(table.clGetDeviceIDs != nullptr);
  CHECK(table.clCreateCommandQueue != nullptr);
  CHECK(table.clEnqueueMapBuffer != nullptr);
  CHECK(table.clCreateUserEvent != nullptr);
  CHECK(table.clCreateImage != nullptr);
  CHECK(table.clSVMAlloc != nullptr);
  CHECK(table.clCreateProgramWithIL != nullptr);
  CHECK(table.clSetProgramReleaseCallback != nullptr);
  CHECK(table.clCreateBufferWithProperties != nullptr); }

TEST(icd_stub_report, filling_a_table_twice_changes_nothing)
{ cl_icd_dispatch first{};
  cl_icd_dispatch second{};
  av::clproto::fill_unimplemented(first);
  av::clproto::fill_unimplemented(second);
  av::clproto::fill_unimplemented(second);
  CHECK(std::memcmp(&first, &second, sizeof first) == 0); }
