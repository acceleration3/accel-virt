#include "core/log/driver_log.hpp"
#include "executor/executor_log.hpp"
#include "executor/opencl/cl_freed_memory.hpp"
#include "executor/opencl/opencl_backend.hpp"
#include "executor/host_ram_ledger.hpp"
#include "harness/executor/fake_memory_gauge.hpp"
#include "executor/vram_ledger.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"

#include <CL/cl.h>
#include <CL/cl_ext.h>

#include <cstdint>
#include <cstring>

using av::OpenClBackend;
using av::VramLedger;

extern "C" const char *__asan_default_options(void);
extern "C" const char *__asan_default_options(void)
{
    return "detect_leaks=0";
}

namespace {

constexpr std::uint64_t gib = 1ull << 30;
constexpr std::uint64_t host_global = 24 * gib;

struct FakeCl {
    std::uint32_t creates = 0u;
    std::uint32_t releases = 0u;
    cl_int        create_error = CL_SUCCESS;
    cl_uint       references = 1u;
    std::uintptr_t next = 0x1000u;
    std::uint64_t  free_kib = 20ull * 1024u * 1024u;
};

FakeCl fake;

cl_mem fake_made(cl_int *errcode_ret)
{
    fake.creates++;
    if (errcode_ret != nullptr) *errcode_ret = fake.create_error;
    if (fake.create_error != CL_SUCCESS) return nullptr;
    fake.next += 0x10u;
    return reinterpret_cast<cl_mem>(fake.next);
}

struct Lines {
    Lines() : sink(), executor(sink), log(executor.opencl()) {}

    av::RecordingLog sink;
    av::ExecutorLog  executor;
    av::DriverLog   &log;
};

class Rig {
public:
    Rig() : lines_(), backend_(lines_.log), ledger_()
    { fake = FakeCl{};
      ledger_.learn(16 * gib);
      backend_.use_vram(&ledger_, nullptr); }
    ~Rig() { backend_.shut_down(); }

    OpenClBackend &backend() { return backend_; }
    VramLedger    &ledger() { return ledger_; }

    cl_mem buffer(std::uint64_t bytes, cl_mem_flags flags, cl_int &error)
    { return backend_.on_clCreateBuffer(nullptr, flags, bytes, nullptr, &error); }

private:
    Lines         lines_;
    OpenClBackend backend_;
    VramLedger    ledger_;
};

}

extern "C" {

CL_API_ENTRY void *CL_API_CALL clSVMAlloc(cl_context, cl_svm_mem_flags, size_t, cl_uint)
{ fake.next += 0x10u;
  return reinterpret_cast<void *>(fake.next); }

CL_API_ENTRY void CL_API_CALL clSVMFree(cl_context, void *) {}

CL_API_ENTRY cl_mem CL_API_CALL clCreateBuffer(cl_context, cl_mem_flags, size_t, void *, cl_int *errcode_ret)
{ return fake_made(errcode_ret); }

CL_API_ENTRY cl_mem CL_API_CALL clCreateImage(cl_context, cl_mem_flags, const cl_image_format *,
                                              const cl_image_desc *, void *, cl_int *errcode_ret)
{ return fake_made(errcode_ret); }

CL_API_ENTRY cl_mem CL_API_CALL clCreateImageWithProperties(cl_context, const cl_mem_properties *, cl_mem_flags,
                                                            const cl_image_format *, const cl_image_desc *, void *,
                                                            cl_int *errcode_ret)
{ return fake_made(errcode_ret); }

CL_API_ENTRY cl_mem CL_API_CALL clCreateImage2D(cl_context, cl_mem_flags, const cl_image_format *, size_t, size_t,
                                                size_t, void *, cl_int *errcode_ret)
{ return fake_made(errcode_ret); }

CL_API_ENTRY cl_mem CL_API_CALL clCreateImage3D(cl_context, cl_mem_flags, const cl_image_format *, size_t, size_t,
                                                size_t, size_t, size_t, void *, cl_int *errcode_ret)
{ return fake_made(errcode_ret); }

CL_API_ENTRY cl_mem CL_API_CALL clCreateBufferWithProperties(cl_context, const cl_mem_properties *, cl_mem_flags,
                                                             size_t, void *, cl_int *errcode_ret)
{ return fake_made(errcode_ret); }

CL_API_ENTRY cl_int CL_API_CALL clReleaseMemObject(cl_mem)
{ fake.releases++;
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clGetMemObjectInfo(cl_mem, cl_mem_info name, size_t size, void *value, size_t *ret)
{ if (name != CL_MEM_REFERENCE_COUNT || size < sizeof(cl_uint)) return CL_INVALID_VALUE;
  *static_cast<cl_uint *>(value) = fake.references;
  if (ret != nullptr) *ret = sizeof(cl_uint);
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clGetDeviceInfo(cl_device_id, cl_device_info name, size_t size, void *value, size_t *ret)
{ if (name == CL_DEVICE_GLOBAL_MEM_SIZE || name == CL_DEVICE_MAX_MEM_ALLOC_SIZE) {
      if (size < sizeof(cl_ulong)) return CL_INVALID_VALUE;
      *static_cast<cl_ulong *>(value) = host_global;
      if (ret != nullptr) *ret = sizeof(cl_ulong);
      return CL_SUCCESS;
  }
  if (name == CL_DEVICE_GLOBAL_FREE_MEMORY_AMD) {
      if (size < 2u * sizeof(size_t)) return CL_INVALID_VALUE;
      static_cast<size_t *>(value)[0] = static_cast<size_t>(fake.free_kib);
      static_cast<size_t *>(value)[1] = static_cast<size_t>(fake.free_kib);
      if (ret != nullptr) *ret = 2u * sizeof(size_t);
      return CL_SUCCESS;
  }
  if (name == CL_DEVICE_MAX_COMPUTE_UNITS) {
      *static_cast<cl_uint *>(value) = 7u;
      return CL_SUCCESS;
  }
  return CL_INVALID_VALUE; }

}

TEST(opencl_backend_vram, a_buffer_under_the_cap_is_made_and_counted_until_its_last_release)
{ Rig rig;
  cl_int error = -1;
  cl_mem const made = rig.buffer(4 * gib, CL_MEM_READ_WRITE, error);
  CHECK(made != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.ledger().used(), 4 * gib);
  CHECK_EQ(rig.backend().on_clReleaseMemObject(made), CL_SUCCESS);
  CHECK_EQ(rig.ledger().used(), 0u);
  CHECK_EQ(fake.releases, 1u); }

TEST(opencl_backend_vram, a_buffer_past_the_cap_fails_with_the_allocation_failure_and_never_reaches_the_driver)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(15 * gib, CL_MEM_READ_WRITE, error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK_EQ(fake.creates, 0u);
  CHECK_EQ(rig.ledger().used(), 0u); }

TEST(opencl_backend_vram, the_context_survives_a_refusal_and_serves_the_next_smaller_buffer)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  cl_mem const big = rig.buffer(10 * gib, CL_MEM_READ_WRITE, error);
  CHECK(big != nullptr);
  CHECK(rig.buffer(5 * gib, CL_MEM_READ_WRITE, error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  cl_mem const small = rig.buffer(3 * gib, CL_MEM_READ_WRITE, error);
  CHECK(small != nullptr);
  CHECK_EQ(error, CL_SUCCESS);
  CHECK_EQ(rig.ledger().used(), 13 * gib); }

TEST(opencl_backend_vram, a_buffer_the_driver_refuses_gives_its_claim_back)
{ Rig rig;
  fake.create_error = CL_OUT_OF_RESOURCES;
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(4 * gib, CL_MEM_READ_WRITE, error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_RESOURCES);
  CHECK_EQ(rig.ledger().used(), 0u); }

TEST(opencl_backend_vram, host_resident_buffers_cost_the_card_nothing)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(30 * gib, CL_MEM_ALLOC_HOST_PTR, error) != nullptr);
  CHECK(rig.buffer(30 * gib, CL_MEM_USE_HOST_PTR, error) != nullptr);
  CHECK_EQ(rig.ledger().used(), 0u); }

TEST(opencl_backend_vram, a_release_that_leaves_another_reference_keeps_the_object_counted)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  cl_mem const made = rig.buffer(2 * gib, CL_MEM_READ_WRITE, error);
  fake.references = 2u;
  CHECK_EQ(rig.backend().on_clReleaseMemObject(made), CL_SUCCESS);
  CHECK_EQ(rig.ledger().used(), 2 * gib);
  fake.references = 1u;
  CHECK_EQ(rig.backend().on_clReleaseMemObject(made), CL_SUCCESS);
  CHECK_EQ(rig.ledger().used(), 0u); }

TEST(opencl_backend_vram, every_image_entry_point_is_counted_by_its_bytes_and_refused_past_the_cap)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  cl_image_format format = {CL_RGBA, CL_FLOAT};
  cl_image_desc desc = {};
  desc.image_type = CL_MEM_OBJECT_IMAGE2D;
  desc.image_width = 16384u;
  desc.image_height = 16384u;
  CHECK(rig.backend().on_clCreateImage(nullptr, CL_MEM_READ_WRITE, &format, &desc, nullptr, &error) != nullptr);
  const std::uint64_t one = 16384ull * 16384ull * 16u;
  CHECK_EQ(rig.ledger().used(), one);
  CHECK(rig.backend().on_clCreateImageWithProperties(nullptr, nullptr, CL_MEM_READ_WRITE, &format, &desc, nullptr,
                                                    &error) != nullptr);
  CHECK_EQ(rig.ledger().used(), 2 * one);
  CHECK(rig.backend().on_clCreateImage2D(nullptr, CL_MEM_READ_WRITE, &format, 16384u, 16384u, 0u, nullptr, &error) !=
        nullptr);
  CHECK_EQ(rig.ledger().used(), 3 * one);
  CHECK(rig.backend().on_clCreateImage3D(nullptr, CL_MEM_READ_WRITE, &format, 16384u, 16384u, 4u, 0u, 0u, nullptr,
                                          &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE);
  CHECK_EQ(rig.ledger().used(), 3 * one); }

TEST(opencl_backend_vram, a_buffer_with_properties_but_no_blob_is_counted_like_any_buffer)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  CHECK(rig.backend().on_clCreateBufferWithProperties(nullptr, nullptr, CL_MEM_READ_WRITE, 3 * gib, nullptr, &error) !=
        nullptr);
  CHECK_EQ(rig.ledger().used(), 3 * gib);
  CHECK(rig.backend().on_clCreateBufferWithProperties(nullptr, nullptr, CL_MEM_READ_WRITE, 12 * gib, nullptr,
                                                      &error) == nullptr);
  CHECK_EQ(error, CL_MEM_OBJECT_ALLOCATION_FAILURE); }

TEST(opencl_backend_vram, shutting_the_backend_down_returns_every_object_the_guest_left_behind)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(2 * gib, CL_MEM_READ_WRITE, error) != nullptr);
  CHECK(rig.buffer(3 * gib, CL_MEM_READ_WRITE, error) != nullptr);
  CHECK_EQ(rig.ledger().used(), 5 * gib);
  rig.backend().shut_down();
  CHECK_EQ(rig.ledger().used(), 0u); }

TEST(opencl_backend_vram, the_global_size_and_largest_allocation_the_guest_reads_are_the_cap)
{ Rig rig;
  cl_ulong global = 0u;
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof global, &global, nullptr),
           CL_SUCCESS);
  CHECK_EQ(global, 14 * gib);
  cl_ulong largest = 0u;
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof largest, &largest, nullptr),
           CL_SUCCESS);
  CHECK_EQ(largest, 14 * gib);
  size_t bytes = 0u;
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_GLOBAL_MEM_SIZE, 0u, nullptr, &bytes), CL_SUCCESS);
  CHECK_EQ(bytes, sizeof(cl_ulong));
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_GLOBAL_MEM_SIZE, 2u, &global, nullptr), CL_INVALID_VALUE); }

TEST(opencl_backend_vram, a_card_smaller_than_the_cap_keeps_its_own_size)
{ Rig rig;
  rig.ledger().learn(64 * gib);
  cl_ulong global = 0u;
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_GLOBAL_MEM_SIZE, sizeof global, &global, nullptr),
           CL_SUCCESS);
  CHECK_EQ(global, host_global); }

TEST(opencl_backend_vram, the_free_memory_the_guest_reads_is_what_is_left_under_the_cap)
{ Rig rig;
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(10 * gib, CL_MEM_READ_WRITE, error) != nullptr);
  size_t free_kib[2] = {};
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_GLOBAL_FREE_MEMORY_AMD, sizeof free_kib, free_kib,
                                            nullptr),
           CL_SUCCESS);
  CHECK_EQ(free_kib[0], 4u * 1024u * 1024u);
  CHECK_EQ(free_kib[1], fake.free_kib); }

TEST(opencl_backend_vram, other_device_queries_pass_through_untouched)
{ Rig rig;
  cl_uint units = 0u;
  CHECK_EQ(rig.backend().on_clGetDeviceInfo(nullptr, CL_DEVICE_MAX_COMPUTE_UNITS, sizeof units, &units, nullptr),
           CL_SUCCESS);
  CHECK_EQ(units, 7u); }

TEST(opencl_backend_host_ram, a_host_resident_buffer_past_the_vms_host_ram_is_out_of_host_memory_and_never_reaches_the_driver)
{ av::HostRamLedger host;
  Rig rig;
  host.budget(8 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  cl_int error = CL_SUCCESS;
  cl_mem const first = rig.buffer(5 * gib, CL_MEM_ALLOC_HOST_PTR, error);
  CHECK(first != nullptr);
  CHECK_EQ(host.used(), 5 * gib);
  CHECK_EQ(rig.ledger().used(), 0u);
  CHECK(rig.buffer(4 * gib, CL_MEM_ALLOC_HOST_PTR, error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(fake.creates, 1u);
  CHECK_EQ(rig.backend().on_clReleaseMemObject(first), CL_SUCCESS);
  CHECK_EQ(host.used(), 0u);
  CHECK(rig.buffer(8 * gib, CL_MEM_ALLOC_HOST_PTR, error) != nullptr); }

TEST(opencl_backend_host_ram, shutting_the_backend_down_gives_the_host_ram_back)
{ av::HostRamLedger host;
  Rig rig;
  host.budget(8 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(3 * gib, CL_MEM_USE_HOST_PTR, error) != nullptr);
  CHECK_EQ(host.used(), 3 * gib);
  rig.backend().shut_down();
  CHECK_EQ(host.used(), 0u); }

TEST(opencl_backend_host_ram, a_driver_that_refuses_a_host_buffer_gives_the_claim_back)
{ av::HostRamLedger host;
  Rig rig;
  host.budget(8 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  fake.create_error = CL_OUT_OF_RESOURCES;
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(4 * gib, CL_MEM_ALLOC_HOST_PTR, error) == nullptr);
  CHECK_EQ(host.used(), 0u); }

TEST(opencl_backend_host_ram, shared_virtual_memory_is_host_ram_and_is_refused_past_the_budget_and_given_back_on_free)
{ av::HostRamLedger host;
  Rig rig;
  host.budget(8 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  void *const first = rig.backend().on_clSVMAlloc(nullptr, 0u, 5 * gib, 0u);
  CHECK(first != nullptr);
  CHECK_EQ(host.used(), 5 * gib);
  CHECK(rig.backend().on_clSVMAlloc(nullptr, 0u, 4 * gib, 0u) == nullptr);
  CHECK_EQ(host.used(), 5 * gib);
  rig.backend().on_clSVMFree(nullptr, first);
  CHECK_EQ(host.used(), 0u);
  CHECK(rig.backend().on_clSVMAlloc(nullptr, 0u, 8 * gib, 0u) != nullptr);
  rig.backend().shut_down();
  CHECK_EQ(host.used(), 0u); }

TEST(opencl_backend_host_ram, a_host_resident_buffer_the_host_cannot_spare_is_out_of_host_memory_and_never_reaches_the_driver)
{ av::HostRamLedger host;
  av::FakeMemoryGauge gauge;
  Rig rig;
  gauge.free_bytes = 5 * gib;
  host.watch(&gauge);
  host.floor(4 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  cl_int error = CL_SUCCESS;
  CHECK(rig.buffer(2 * gib, CL_MEM_ALLOC_HOST_PTR, error) == nullptr);
  CHECK_EQ(error, CL_OUT_OF_HOST_MEMORY);
  CHECK_EQ(fake.creates, 0u);
  gauge.set(20 * gib, av::HostRamLedger::refresh_ms);
  CHECK(rig.buffer(2 * gib, CL_MEM_ALLOC_HOST_PTR, error) != nullptr); }

TEST(opencl_backend_freed, a_released_buffer_a_freed_svm_block_and_what_a_shut_down_session_left_are_told_to_the_shared_total)
{ av::ClFreedMemory shared;
  av::HostRamLedger host;
  Rig rig;
  host.budget(8 * gib);
  rig.backend().use_vram(&rig.ledger(), &host);
  rig.backend().use_freed(&shared);
  cl_int error = CL_SUCCESS;
  cl_mem const first = rig.buffer(1u << 20, CL_MEM_READ_WRITE, error);
  CHECK(first != nullptr);
  CHECK_EQ(shared.total(), 0u);
  CHECK_EQ(rig.backend().on_clReleaseMemObject(first), CL_SUCCESS);
  CHECK_EQ(shared.total(), 2u << 20);
  void *const block = rig.backend().on_clSVMAlloc(nullptr, 0u, 3u << 20, 0u);
  CHECK(block != nullptr);
  rig.backend().on_clSVMFree(nullptr, block);
  CHECK_EQ(shared.total(), (2u << 20) + (8u << 20));
  CHECK(rig.buffer(1u << 20, CL_MEM_READ_WRITE, error) != nullptr);
  rig.backend().shut_down();
  CHECK_EQ(shared.total(), (2u << 20) + (8u << 20) + (2u << 20)); }

TEST(opencl_backend_freed, a_buffer_the_guest_still_holds_a_reference_to_is_not_freed_yet)
{ av::ClFreedMemory shared;
  Rig rig;
  rig.backend().use_freed(&shared);
  cl_int error = CL_SUCCESS;
  cl_mem const held = rig.buffer(4096u, CL_MEM_READ_WRITE, error);
  fake.references = 2u;
  CHECK_EQ(rig.backend().on_clReleaseMemObject(held), CL_SUCCESS);
  CHECK_EQ(shared.total(), 0u); }
