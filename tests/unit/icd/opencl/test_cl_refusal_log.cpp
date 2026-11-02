#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_refusal_log.hpp"

#include <cstdint>
#include <cstring>

using av::clicd::ClRefusalLog;

namespace {

const char image_2d[] = "clCreateImage2D";
const char mem_info[] = "clGetMemObjectInfo";

}

TEST(cl_refusal_log, each_refused_entry_point_is_named_once_and_in_the_order_called)
{ av::MockPlatform        platform;
  av::RecordingLog        log;
  ClRefusalLog            refusals(platform, log);
  refusals.note(image_2d);
  refusals.note(mem_info);
  refusals.note(image_2d);
  refusals.note(mem_info);
  CHECK_EQ(log.lines(), 2u);
  CHECK_EQ(refusals.named(), 2u);
  CHECK(std::strcmp(log.line(0u), "avcl: an entry point this ICD does not serve was called: clCreateImage2D") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: an entry point this ICD does not serve was called: clGetMemObjectInfo") == 0);
  }

TEST(cl_refusal_log, nothing_is_named_for_no_entry_point_and_every_one_past_the_old_bound_is)
{ av::MockPlatform        platform;
  av::RecordingLog        log;
  ClRefusalLog            refusals(platform, log);
  refusals.note(nullptr);
  CHECK_EQ(log.lines(), 0u);
  constexpr std::uint32_t many = 1000u;
  static char names[many][4] = {};
  for (std::uint32_t index = 0u; index < many; ++index) refusals.note(names[index]);
  for (std::uint32_t index = 0u; index < many; ++index) refusals.note(names[index]);
  CHECK_EQ(log.lines(), many);
  CHECK_EQ(refusals.named(), many); }

TEST(cl_refusal_log, each_error_an_entry_point_answers_is_named_once_and_success_never)
{ av::MockPlatform        platform;
  av::RecordingLog        log;
  ClRefusalLog            refusals(platform, log);
  refusals.note_failure(image_2d, -38);
  refusals.note_failure(image_2d, -38);
  refusals.note_failure(image_2d, -30);
  refusals.note_failure(mem_info, -38);
  refusals.note_failure(mem_info, 0);
  refusals.note_failure(mem_info, 5);
  refusals.note_failure(nullptr, -38);
  CHECK_EQ(log.lines(), 3u);
  CHECK(std::strcmp(log.line(0u), "avcl: clCreateImage2D answered -38") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: clCreateImage2D answered -30") == 0);
  CHECK(std::strcmp(log.line(2u), "avcl: clGetMemObjectInfo answered -38") == 0);
  refusals.note(image_2d);
  CHECK_EQ(log.lines(), 4u); }

TEST(cl_refusal_log, each_query_an_entry_point_refuses_is_named_with_it_once)
{ av::MockPlatform        platform;
  av::RecordingLog        log;
  ClRefusalLog            refusals(platform, log);
  refusals.note_query(mem_info, 0x4039u, -30);
  refusals.note_query(mem_info, 0x4039u, -30);
  refusals.note_query(mem_info, 0x1002u, -30);
  refusals.note_query(mem_info, 0x1002u, 0);
  refusals.note_query(nullptr, 0x4039u, -30);
  CHECK_EQ(log.lines(), 2u);
  CHECK(std::strcmp(log.line(0u), "avcl: clGetMemObjectInfo 0x4039 answered -30") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: clGetMemObjectInfo 0x1002 answered -30") == 0); }

TEST(cl_refusal_log, a_refused_map_and_a_buffer_released_mapped_say_how_many_were_held)
{ av::MockPlatform platform;
  av::RecordingLog log;
  ClRefusalLog     refusals(platform, log);
  static const char slot[] = "clEnqueueMapBuffer: no slot";
  refusals.note_map(slot, 4096u, 64u);
  refusals.note_map(slot, 8u, 65u);
  refusals.note_map(nullptr, 1u, 1u);
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strcmp(log.line(0u), "avcl: clEnqueueMapBuffer: no slot of 4096 bytes, with 64 mappings held") == 0);
  refusals.note_released_mapped(0u);
  CHECK_EQ(log.lines(), 1u);
  refusals.note_released_mapped(3u);
  refusals.note_released_mapped(5u);
  CHECK_EQ(log.lines(), 2u);
  CHECK(std::strcmp(log.line(1u), "avcl: clReleaseMemObject: a buffer released while still mapped 3 mappings went with it") == 0); }

TEST(cl_refusal_log, a_transfer_that_ran_ahead_of_a_user_event_is_said_once)
{ av::MockPlatform platform;
  av::RecordingLog log;
  ClRefusalLog     refusals(platform, log);
  refusals.note_ran_ahead_of_user_event();
  refusals.note_ran_ahead_of_user_event();
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strstr(log.line(0u), "a non-blocking transfer waits on a user event not yet set") != nullptr);
  CHECK(std::strstr(log.line(0u), "it runs now") != nullptr); }

