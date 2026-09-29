/* ClInfoRequest: a clGet*Info's answer lands whole in room enough, a size is
 * said whenever asked, too little room writes nothing and is refused, and no
 * place for the answer asks the size alone. */
#include "harness/framework/test_registry.hpp"
#include "icd/opencl/cl_info_request.hpp"

#include <cstddef>
#include <cstdint>

using av::clicd::ClInfoRequest;

TEST(cl_info_request, an_answer_lands_whole_and_says_its_size)
{ const std::uint64_t answer = 0x1122334455667788ull;
  std::uint64_t       out = 0u;
  std::size_t         size = 0u;
  CHECK_EQ(ClInfoRequest(sizeof out, &out, &size).give(&answer, sizeof answer), CL_SUCCESS);
  CHECK_EQ(out, answer);
  CHECK_EQ(size, sizeof answer);
  /* More room than the answer is fine, and no size asked is fine. */
  std::uint64_t wide[2] = {0u, 5u};
  CHECK_EQ(ClInfoRequest(sizeof wide, wide, nullptr).give(&answer, sizeof answer), CL_SUCCESS);
  CHECK_EQ(wide[0], answer);
  CHECK_EQ(wide[1], 5u); }

TEST(cl_info_request, too_little_room_writes_nothing_and_no_room_asks_the_size)
{ const std::uint64_t answer = 0x1122334455667788ull;
  std::uint32_t       narrow = 7u;
  std::size_t         size = 3u;
  CHECK_EQ(ClInfoRequest(sizeof narrow, &narrow, &size).give(&answer, sizeof answer), CL_INVALID_VALUE);
  CHECK_EQ(narrow, 7u);
  CHECK_EQ(size, 3u);
  CHECK_EQ(ClInfoRequest(0u, nullptr, &size).give(&answer, sizeof answer), CL_SUCCESS);
  CHECK_EQ(size, sizeof answer);
  /* An empty answer is a size of 0. */
  CHECK_EQ(ClInfoRequest(0u, nullptr, &size).give(nullptr, 0u), CL_SUCCESS);
  CHECK_EQ(size, 0u); }
