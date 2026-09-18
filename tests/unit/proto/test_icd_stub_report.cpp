/* The generated OpenCL ICD table, compiled for the first time.
 *
 * `cl_icd_stubs.h` fills every dispatch slot the driver did not claim, because
 * the Khronos loader reads a slot and calls through it without checking - a
 * null slot is a jump into nothing with no diagnostic. What matters here: a
 * slot the driver claimed is left alone, every slot the codegen knows about is
 * filled, a refusal answers what the API means by failure through both a
 * return value and an errcode_ret, and each refusal names itself once.
 *
 * Nothing had ever included this header, so the `av::icd::stub_called` it calls
 * was declared nowhere (feature 052, research §3). The definition here is the
 * test's own; the ICD's is in src/icd/opencl/cl_hooks.cpp. */
#include "harness/framework/test_registry.hpp"
#include "proto/generated/opencl/cl_icd_stubs.h"
#include "proto/icd_stub_report.hpp"

#include <cstring>

namespace {

/* What the refusals said, for the test to read back. A test binary may keep
 * this at file scope; src/ may not (constitution III), which is why the ICD
 * reaches its log through its binding instead. */
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

/* The one slot this fake driver claims, so the fill has something to leave
 * alone. Its answer is one no refusal returns. */
cl_int CL_API_CALL claimed_get_platform_ids(cl_uint, cl_platform_id *, cl_uint *)
{
    return CL_PLATFORM_NOT_FOUND_KHR;
}

}  // namespace

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

/* Not called here; the OpenCL client never reports a dropped payload. Defined
 * so a translation unit that links this one links at all. */
void payload_dropped(const char *, const char *, size_t) noexcept {}

}  // namespace av::icd

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
  /* A null errcode_ret is what most callers pass, and must not be written. */
  CHECK(table.clCreateContext(nullptr, 0u, nullptr, nullptr, nullptr, nullptr) == nullptr); }

TEST(icd_stub_report, every_core_slot_the_loader_dispatches_through_is_filled)
{ cl_icd_dispatch table{};
  av::clproto::fill_unimplemented(table);
  /* One from each version the header gates, so a gate that stopped filling
   * shows. These are core entry points the loader exports and dispatches
   * straight through, where a null slot is a crash rather than an error. */
  CHECK(table.clGetPlatformInfo != nullptr);
  CHECK(table.clGetDeviceIDs != nullptr);
  CHECK(table.clCreateCommandQueue != nullptr);
  CHECK(table.clEnqueueMapBuffer != nullptr);
  CHECK(table.clCreateUserEvent != nullptr);      /* 1.1 */
  CHECK(table.clCreateImage != nullptr);          /* 1.2 */
  CHECK(table.clSVMAlloc != nullptr);             /* 2.0 */
  CHECK(table.clCreateProgramWithIL != nullptr);  /* 2.1 */
  CHECK(table.clSetProgramReleaseCallback != nullptr); /* 2.2 */
  CHECK(table.clCreateBufferWithProperties != nullptr); /* 3.0 */ }

TEST(icd_stub_report, filling_a_table_twice_changes_nothing)
{ cl_icd_dispatch first{};
  cl_icd_dispatch second{};
  av::clproto::fill_unimplemented(first);
  av::clproto::fill_unimplemented(second);
  av::clproto::fill_unimplemented(second);
  CHECK(std::memcmp(&first, &second, sizeof first) == 0); }
