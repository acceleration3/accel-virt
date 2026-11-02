#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_info_answers.hpp"
#include "icd/opencl/cl_info_request.hpp"
#include "icd/opencl/cl_object.hpp"

#include <cstdint>
#include <cstring>

using av::clicd::ClInfoAnswers;
using av::clicd::ClInfoRequest;
using av::clicd::ClObject;

namespace {

constexpr std::uint64_t device = 0x4400u;

}

TEST(cl_info_answers, an_answer_kept_is_given_for_its_object_and_name_alone)
{ ClInfoAnswers<4u, 16u> answers;
  std::uint32_t          got = 0u;
  std::size_t            size = 0u;
  cl_int                 result = CL_INVALID_OPERATION;
  CHECK(!answers.answer(device, CL_DEVICE_MAX_COMPUTE_UNITS, ClInfoRequest(sizeof got, &got, &size), result));
  CHECK_EQ(result, CL_INVALID_OPERATION);
  const std::uint32_t units = 64u;
  answers.keep(device, CL_DEVICE_MAX_COMPUTE_UNITS, &units, sizeof units);
  CHECK(answers.answer(device, CL_DEVICE_MAX_COMPUTE_UNITS, ClInfoRequest(sizeof got, &got, &size), result));
  CHECK_EQ(result, CL_SUCCESS);
  CHECK_EQ(got, 64u);
  CHECK_EQ(size, sizeof units);
  CHECK(!answers.answer(device, CL_DEVICE_MAX_CLOCK_FREQUENCY, ClInfoRequest(sizeof got, &got, &size), result));
  CHECK(!answers.answer(device + 1u, CL_DEVICE_MAX_COMPUTE_UNITS, ClInfoRequest(sizeof got, &got, &size), result));
  size = 0u;
  CHECK(answers.answer(device, CL_DEVICE_MAX_COMPUTE_UNITS, ClInfoRequest(0u, nullptr, &size), result));
  CHECK_EQ(result, CL_SUCCESS);
  CHECK_EQ(size, sizeof units);
  std::uint16_t cramped = 0x5A5Au;
  CHECK(answers.answer(device, CL_DEVICE_MAX_COMPUTE_UNITS, ClInfoRequest(sizeof cramped, &cramped, nullptr),
                       result));
  CHECK_EQ(result, CL_INVALID_VALUE);
  CHECK_EQ(cramped, 0x5A5Au);
  CHECK_EQ(answers.kept(), 1u); }

TEST(cl_info_answers, an_answer_as_long_as_the_room_is_kept_and_a_longer_one_is_not)
{ ClInfoAnswers<4u, 8u> answers;
  const char            whole[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', '\0'};
  const char            longer[9] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', '\0'};
  answers.keep(device, CL_DEVICE_NAME, whole, sizeof whole);
  answers.keep(device, CL_DEVICE_VENDOR, longer, sizeof longer);
  char   got[16] = {};
  cl_int result = CL_INVALID_OPERATION;
  CHECK(answers.answer(device, CL_DEVICE_NAME, ClInfoRequest(sizeof got, got, nullptr), result));
  CHECK_EQ(std::strcmp(got, "abcdefg"), 0);
  CHECK(!answers.answer(device, CL_DEVICE_VENDOR, ClInfoRequest(sizeof got, got, nullptr), result));
  CHECK_EQ(answers.kept(), 1u); }

TEST(cl_info_answers, a_full_table_keeps_nothing_more_and_still_answers)
{ ClInfoAnswers<2u, 8u> answers;
  const std::uint64_t   one = 1u, two = 2u, three = 3u;
  answers.keep(device, 1u, &one, sizeof one);
  answers.keep(device, 1u, &two, sizeof two);
  answers.keep(device, 3u, &three, sizeof three);
  CHECK_EQ(answers.kept(), 2u);
  std::uint64_t got = 0u;
  cl_int        result = CL_INVALID_OPERATION;
  CHECK(answers.answer(device, 1u, ClInfoRequest(sizeof got, &got, nullptr), result));
  CHECK_EQ(got, 1u);
  CHECK(!answers.answer(device, 3u, ClInfoRequest(sizeof got, &got, nullptr), result));
  answers.keep(device, 4u, &three, sizeof three);
  CHECK_EQ(answers.kept(), 2u); }

TEST(cl_info_answers, each_mem_object_keeps_its_own_answers)
{ const cl_icd_dispatch table = {};
  ClObject              first(table, ClObject::Kind::mem, 0x6600u, nullptr, nullptr);
  ClObject              second(table, ClObject::Kind::mem, 0x6601u, nullptr, nullptr);
  const std::size_t     size = 4096u;
  first.keep_answer(CL_MEM_SIZE, &size, sizeof size);
  std::size_t got = 0u;
  cl_int      result = CL_INVALID_OPERATION;
  CHECK(first.kept_answer(CL_MEM_SIZE, ClInfoRequest(sizeof got, &got, nullptr), result));
  CHECK_EQ(result, CL_SUCCESS);
  CHECK_EQ(got, 4096u);
  CHECK(!second.kept_answer(CL_MEM_SIZE, ClInfoRequest(sizeof got, &got, nullptr), result));
  CHECK(!first.kept_answer(CL_MEM_FLAGS, ClInfoRequest(sizeof got, &got, nullptr), result)); }
