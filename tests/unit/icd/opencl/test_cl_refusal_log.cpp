#include "core/log/keyed_log.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opencl/cl_refusal_log.hpp"

#include <cstdint>
#include <cstring>

using av::clicd::ClRefusalLog;

namespace {

const char mem_info[] = "clGetMemObjectInfo";

}

TEST(cl_refusal_log, each_query_an_entry_point_refuses_is_named_with_it_once)
{ av::MockPlatform        platform;
  av::RecordingLog        log;
  av::KeyedLog            lines(platform, log, "avcl: ");
  ClRefusalLog            refusals(lines);
  refusals.note_query(mem_info, 0x4039u, -30);
  refusals.note_query(mem_info, 0x4039u, -30);
  refusals.note_query(mem_info, 0x1002u, -30);
  refusals.note_query(mem_info, 0x1002u, 0);
  refusals.note_query(nullptr, 0x4039u, -30);
  CHECK_EQ(log.lines(), 2u);
  CHECK(std::strcmp(log.line(0u), "avcl: clGetMemObjectInfo 0x4039 answered -30") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: clGetMemObjectInfo 0x1002 answered -30") == 0);
  CHECK_EQ(refusals.named(), 2u); }

TEST(cl_refusal_log, a_refused_map_and_a_buffer_released_mapped_say_how_many_were_held)
{ av::MockPlatform platform;
  av::RecordingLog log;
  av::KeyedLog     lines(platform, log, "avcl: ");
  ClRefusalLog     refusals(lines);
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
  av::KeyedLog     lines(platform, log, "avcl: ");
  ClRefusalLog     refusals(lines);
  refusals.note_ran_ahead_of_user_event();
  refusals.note_ran_ahead_of_user_event();
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strstr(log.line(0u), "a non-blocking transfer waits on a user event not yet set") != nullptr);
  CHECK(std::strstr(log.line(0u), "it runs now") != nullptr); }

