#include "core/platform/poll_clock.hpp"
#include "executor/opencl/cl_freed_memory.hpp"
#include "executor/opencl/cl_fresh_bytes.hpp"
#include "executor/opencl/cl_queue_wait.hpp"
#include "harness/framework/test_registry.hpp"

#include <CL/cl.h>
#include <cstdint>
#include <cstring>

namespace {

struct Fake {
    int            queues_made = 0;
    int            queues_released = 0;
    int            marker_polls_until_done = 0;
    int            marker_polls = 0;
    cl_int         fill_answer = CL_SUCCESS;
    cl_int         queue_error = CL_SUCCESS;
    std::size_t    fill_pattern = 0u;
    std::size_t    fill_bytes = 0u;
    std::size_t    fill_offset = 99u;
    std::uint8_t   fill_first = 0xFFu;
    int            fills = 0;
    int            svm_fills = 0;
    int            image_fills = 0;
    std::size_t    region[3] = {};
    cl_mem_object_type type = CL_MEM_OBJECT_IMAGE2D;
    cl_mem         associated = nullptr;
    std::size_t    width = 4u, height = 5u, depth = 0u, layers = 0u;
    std::size_t    image_bytes = 0u;
};

Fake fake;

void reset() { fake = Fake(); }

class StepClock final : public av::PollClock {
public:
    std::uint64_t now_nanoseconds() const override { now_ += 100000u; return now_; }
    void          pause(std::uint64_t nanoseconds) override { now_ += nanoseconds; }
    mutable std::uint64_t now_ = 1u;
};

cl_context      context_a() { return reinterpret_cast<cl_context>(std::uintptr_t{0x10}); }
cl_context      context_b() { return reinterpret_cast<cl_context>(std::uintptr_t{0x20}); }
cl_device_id    device() { return reinterpret_cast<cl_device_id>(std::uintptr_t{0x30}); }
cl_mem          memory() { return reinterpret_cast<cl_mem>(std::uintptr_t{0x40}); }

struct Rig {
    Rig() : clock(), wait(clock), fresh(wait) { reset(); }
    StepClock          clock;
    av::ClQueueWait    wait;
    av::ClFreshBytes   fresh;
};

}

extern "C" {

CL_API_ENTRY cl_command_queue CL_API_CALL clCreateCommandQueueWithProperties(cl_context, cl_device_id,
                                                                              const cl_queue_properties *,
                                                                              cl_int *errcode_ret)
{ if (errcode_ret != nullptr) *errcode_ret = fake.queue_error;
  if (fake.queue_error != CL_SUCCESS) return nullptr;
  fake.queues_made++;
  return reinterpret_cast<cl_command_queue>(std::uintptr_t{0x100} + static_cast<std::uintptr_t>(fake.queues_made)); }

CL_API_ENTRY cl_int CL_API_CALL clReleaseCommandQueue(cl_command_queue)
{ fake.queues_released++;
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clEnqueueFillBuffer(cl_command_queue, cl_mem, const void *pattern,
                                                    size_t pattern_size, size_t offset, size_t size, cl_uint,
                                                    const cl_event *, cl_event *)
{ fake.fills++;
  fake.fill_pattern = pattern_size;
  fake.fill_offset = offset;
  fake.fill_bytes = size;
  fake.fill_first = *static_cast<const std::uint8_t *>(pattern);
  return fake.fill_answer; }

CL_API_ENTRY cl_int CL_API_CALL clEnqueueSVMMemFill(cl_command_queue, void *, const void *, size_t pattern_size,
                                                    size_t size, cl_uint, const cl_event *, cl_event *)
{ fake.svm_fills++;
  fake.fill_pattern = pattern_size;
  fake.fill_bytes = size;
  return fake.fill_answer; }

CL_API_ENTRY cl_int CL_API_CALL clEnqueueFillImage(cl_command_queue, cl_mem, const void *, const size_t *,
                                                   const size_t *region, cl_uint, const cl_event *, cl_event *)
{ fake.image_fills++;
  std::memcpy(fake.region, region, sizeof fake.region);
  return fake.fill_answer; }

CL_API_ENTRY cl_int CL_API_CALL clGetMemObjectInfo(cl_mem, cl_mem_info name, size_t, void *value, size_t *)
{ if (name == CL_MEM_TYPE) *static_cast<cl_mem_object_type *>(value) = fake.type;
  else if (name == CL_MEM_ASSOCIATED_MEMOBJECT) *static_cast<cl_mem *>(value) = fake.associated;
  else if (name == CL_MEM_SIZE && fake.image_bytes != 0u) *static_cast<std::size_t *>(value) = fake.image_bytes;
  else return CL_INVALID_VALUE;
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clGetImageInfo(cl_mem, cl_image_info name, size_t, void *value, size_t *)
{ size_t *const out = static_cast<size_t *>(value);
  if (name == CL_IMAGE_WIDTH) *out = fake.width;
  else if (name == CL_IMAGE_HEIGHT) *out = fake.height;
  else if (name == CL_IMAGE_DEPTH) *out = fake.depth;
  else if (name == CL_IMAGE_ARRAY_SIZE) *out = fake.layers;
  else return CL_INVALID_VALUE;
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clEnqueueMarkerWithWaitList(cl_command_queue, cl_uint, const cl_event *,
                                                            cl_event *event)
{ *event = reinterpret_cast<cl_event>(&fake);
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clFlush(cl_command_queue) { return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clGetEventInfo(cl_event, cl_event_info, size_t, void *value, size_t *)
{ fake.marker_polls++;
  *static_cast<cl_int *>(value) = fake.marker_polls > fake.marker_polls_until_done ? CL_COMPLETE : CL_RUNNING;
  return CL_SUCCESS; }

CL_API_ENTRY cl_int CL_API_CALL clReleaseEvent(cl_event) { return CL_SUCCESS; }

}

TEST(cl_fresh_bytes, a_buffer_is_filled_whole_with_zero_in_the_widest_pattern_its_size_allows)
{ Rig rig;
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK_EQ(fake.fills, 1);
  CHECK_EQ(fake.fill_bytes, 4096u);
  CHECK_EQ(fake.fill_offset, 0u);
  CHECK_EQ(fake.fill_pattern, 128u);
  CHECK_EQ(fake.fill_first, 0u);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 12u));
  CHECK_EQ(fake.fill_pattern, 4u);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 7u));
  CHECK_EQ(fake.fill_pattern, 1u);
  CHECK_EQ(rig.fresh.zeroings(), 3u); }

TEST(cl_fresh_bytes, the_fill_has_finished_before_the_call_returns)
{ Rig rig;
  fake.marker_polls_until_done = 3;
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK(fake.marker_polls > 3); }

TEST(cl_fresh_bytes, a_fill_that_never_finishes_fails_instead_of_waiting_forever)
{ Rig rig;
  fake.marker_polls_until_done = 1 << 30;
  CHECK(!rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK_EQ(rig.fresh.zeroings(), 0u); }

TEST(cl_fresh_bytes, a_refused_fill_or_queue_fails_and_counts_nothing)
{ Rig rig;
  fake.fill_answer = CL_OUT_OF_RESOURCES;
  CHECK(!rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK(!rig.fresh.zero_svm(context_a(), device(), memory(), 4096u));
  CHECK(!rig.fresh.zero_image(context_a(), device(), memory()));
  rig.fresh.release();
  reset();
  fake.queue_error = CL_OUT_OF_HOST_MEMORY;
  CHECK(!rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK(!rig.fresh.zero_svm(context_a(), device(), memory(), 4096u));
  CHECK(!rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(rig.fresh.zeroings(), 0u); }

TEST(cl_fresh_bytes, nothing_is_filled_for_an_empty_buffer)
{ Rig rig;
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 0u));
  CHECK(rig.fresh.zero_svm(context_a(), device(), memory(), 0u));
  CHECK_EQ(fake.fills + fake.svm_fills, 0); }

TEST(cl_fresh_bytes, one_queue_serves_a_context_and_a_new_context_replaces_it)
{ Rig rig;
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK_EQ(fake.queues_made, 1);
  CHECK(rig.fresh.zero_buffer(context_b(), device(), memory(), 4096u));
  CHECK_EQ(fake.queues_made, 2);
  CHECK_EQ(fake.queues_released, 1);
  rig.fresh.release();
  CHECK_EQ(fake.queues_released, 2);
  rig.fresh.release();
  CHECK_EQ(fake.queues_released, 2); }

TEST(cl_fresh_bytes, svm_memory_is_filled_through_the_svm_fill)
{ Rig rig;
  CHECK(rig.fresh.zero_svm(context_a(), device(), memory(), 256u));
  CHECK_EQ(fake.svm_fills, 1);
  CHECK_EQ(fake.fill_bytes, 256u);
  CHECK_EQ(rig.fresh.zeroings(), 1u); }

TEST(cl_fresh_bytes, an_image_is_filled_over_its_whole_extent_by_type)
{ Rig rig;
  fake.type = CL_MEM_OBJECT_IMAGE2D;
  fake.width = 4u; fake.height = 5u; fake.depth = 0u;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.region[0], 4u);
  CHECK_EQ(fake.region[1], 5u);
  CHECK_EQ(fake.region[2], 1u);
  fake.type = CL_MEM_OBJECT_IMAGE1D;
  fake.height = 0u;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.region[1], 1u);
  fake.type = CL_MEM_OBJECT_IMAGE1D_ARRAY;
  fake.layers = 6u;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.region[1], 6u);
  CHECK_EQ(fake.region[2], 1u);
  fake.type = CL_MEM_OBJECT_IMAGE2D_ARRAY;
  fake.height = 5u;
  fake.layers = 3u;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.region[1], 5u);
  CHECK_EQ(fake.region[2], 3u);
  fake.type = CL_MEM_OBJECT_IMAGE3D;
  fake.depth = 7u;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.region[2], 7u);
  CHECK_EQ(fake.image_fills, 5); }

TEST(cl_fresh_bytes, an_image_over_another_object_s_bytes_is_left_to_that_object)
{ Rig rig;
  fake.associated = memory();
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  fake.associated = nullptr;
  fake.type = CL_MEM_OBJECT_IMAGE1D_BUFFER;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.image_fills, 0); }

TEST(cl_fresh_bytes, memory_nobody_else_freed_is_not_filled)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK(rig.fresh.zero_svm(context_a(), device(), memory(), 4096u));
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.fills + fake.svm_fills + fake.image_fills, 0);
  CHECK_EQ(rig.fresh.skipped(), 3u);
  CHECK_EQ(rig.fresh.zeroings(), 0u); }

TEST(cl_fresh_bytes, what_an_owner_freed_itself_never_makes_its_own_memory_dirty)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  rig.fresh.freed(1u << 20);
  rig.fresh.freed(8u << 20);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 16u << 20));
  CHECK_EQ(fake.fills, 0);
  CHECK_EQ(rig.fresh.skipped(), 2u); }

TEST(cl_fresh_bytes, anything_small_is_filled_once_another_owner_freed_something)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  Rig other;
  other.fresh.share(&shared);
  other.fresh.freed(100u);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK_EQ(fake.fills, 1);
  CHECK(rig.fresh.zero_svm(context_a(), device(), memory(), (4u << 20) - 1u));
  CHECK_EQ(fake.svm_fills, 1);
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.image_fills, 1);
  CHECK_EQ(rig.fresh.skipped(), 0u); }

TEST(cl_fresh_bytes, a_large_allocation_is_filled_only_when_others_freed_at_least_its_size_rounded_up_twice)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  Rig other;
  other.fresh.share(&shared);
  other.fresh.freed(5u << 20);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), (16u << 20) + 1u));
  CHECK_EQ(fake.fills, 0);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 16u << 20));
  CHECK_EQ(fake.fills, 1);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 32u << 20));
  CHECK_EQ(fake.fills, 1);
  other.fresh.freed(8u << 20);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 32u << 20));
  CHECK_EQ(fake.fills, 2);
  CHECK_EQ(rig.fresh.skipped(), 2u); }

TEST(cl_fresh_bytes, a_session_that_leaves_makes_what_it_freed_foreign_to_the_next_one)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  rig.fresh.freed(1u << 20);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK_EQ(fake.fills, 0);
  rig.fresh.leave();
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 4096u));
  CHECK_EQ(fake.fills, 1); }

TEST(cl_fresh_bytes, a_total_that_saturated_fills_everything)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  shared.add(av::ClFreedMemory::saturated);
  CHECK(rig.fresh.zero_buffer(context_a(), device(), memory(), 64u << 20));
  CHECK_EQ(fake.fills, 1);
  Rig big;
  big.fresh.share(&shared);
  big.fresh.freed(~std::uint64_t{0});
  CHECK(big.fresh.zero_buffer(context_a(), device(), memory(), 64u << 20));
  CHECK_EQ(fake.fills, 1); }

TEST(cl_fresh_bytes, an_image_whose_size_is_unknown_is_filled_when_anyone_else_freed)
{ Rig rig;
  av::ClFreedMemory shared;
  rig.fresh.share(&shared);
  Rig other;
  other.fresh.share(&shared);
  other.fresh.freed(1u);
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.image_fills, 1);
  fake.image_bytes = 64u << 20;
  CHECK(rig.fresh.zero_image(context_a(), device(), memory()));
  CHECK_EQ(fake.image_fills, 1); }
