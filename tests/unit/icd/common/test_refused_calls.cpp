#include "core/log/keyed_log.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/recording_log.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/common/refused_calls.hpp"

#include <cstdint>
#include <cstring>

using av::icd::RefusedCalls;

namespace {

const char image_2d[] = "clCreateImage2D";
const char mem_info[] = "clGetMemObjectInfo";

}

TEST(refused_calls, each_unserved_entry_point_is_named_once_and_in_the_order_called)
{ av::MockPlatform platform;
  av::RecordingLog log;
  av::KeyedLog     lines(platform, log, "avcl: ");
  RefusedCalls     refused(lines);
  refused.unserved(image_2d);
  refused.unserved(mem_info);
  refused.unserved(image_2d);
  refused.unserved(mem_info);
  refused.unserved(nullptr);
  CHECK_EQ(log.lines(), 2u);
  CHECK_EQ(lines.keys(), 2u);
  CHECK(std::strcmp(log.line(0u), "avcl: an entry point this driver does not serve was called: clCreateImage2D") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: an entry point this driver does not serve was called: clGetMemObjectInfo") == 0); }

TEST(refused_calls, every_unserved_entry_point_past_any_fixed_bound_is_named)
{ av::MockPlatform platform;
  av::RecordingLog log;
  av::KeyedLog     lines(platform, log, "avcl: ");
  RefusedCalls     refused(lines);
  constexpr std::uint32_t many = 1000u;
  static char names[many][4] = {};
  for (std::uint32_t index = 0u; index < many; ++index) refused.unserved(names[index]);
  for (std::uint32_t index = 0u; index < many; ++index) refused.unserved(names[index]);
  CHECK_EQ(lines.keys(), many); }

TEST(refused_calls, each_code_an_entry_point_answers_is_named_once)
{ av::MockPlatform platform;
  av::RecordingLog log;
  av::KeyedLog     lines(platform, log, "avcl: ");
  RefusedCalls     refused(lines);
  refused.failed(image_2d, -38);
  refused.failed(image_2d, -38);
  refused.failed(image_2d, -30);
  refused.failed(mem_info, -38);
  refused.failed(mem_info, 5);
  refused.failed(nullptr, -38);
  CHECK_EQ(log.lines(), 4u);
  CHECK(std::strcmp(log.line(0u), "avcl: clCreateImage2D answered -38") == 0);
  CHECK(std::strcmp(log.line(1u), "avcl: clCreateImage2D answered -30") == 0);
  CHECK(std::strcmp(log.line(2u), "avcl: clGetMemObjectInfo answered -38") == 0);
  CHECK(std::strcmp(log.line(3u), "avcl: clGetMemObjectInfo answered 5") == 0); }

TEST(refused_calls, an_hresult_is_said_in_hex)
{ av::MockPlatform platform;
  av::RecordingLog log;
  av::KeyedLog     lines(platform, log, "avd3d: ");
  RefusedCalls     refused(lines);
  refused.failed(image_2d, static_cast<std::int32_t>(0x887A0005u));
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strstr(log.line(0u), "clCreateImage2D answered ") != nullptr);
  CHECK(std::strstr(log.line(0u), "887A0005") != nullptr || std::strstr(log.line(0u), "887a0005") != nullptr); }

TEST(refused_calls, a_dropped_command_is_said_once_with_where_its_encoding_overflowed)
{ av::MockPlatform platform;
  av::RecordingLog log;
  av::KeyedLog     lines(platform, log, "avgl: ");
  RefusedCalls     refused(lines);
  refused.dropped("OpenGL", mem_info, 4096u);
  refused.dropped("OpenGL", mem_info, 8192u);
  refused.dropped("OpenGL", nullptr, 1u);
  CHECK_EQ(log.lines(), 1u);
  CHECK(std::strcmp(log.line(0u), "avgl: OpenGL clGetMemObjectInfo not sent: its encoding overflowed at byte 4096") == 0);
  refused.failed(mem_info, -1);
  CHECK_EQ(log.lines(), 2u); }
