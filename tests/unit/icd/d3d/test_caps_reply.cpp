/* One GetCaps answer into the runtime's buffer (CapsReply): a whole answer
 * only where it fits, a prefix answer as far as the buffer goes, the rest of
 * the buffer zeroed either way, and no buffer refused. */
#include "harness/framework/test_registry.hpp"
#include "icd/d3d/caps_reply.hpp"

#include <cstdint>
#include <cstring>

using av::d3d::CapsReply;

namespace {

struct Options {
    std::uint32_t first;
    std::uint32_t second;
    std::uint32_t third;
};

const Options answer = {0x11u, 0x22u, 0x33u};

}  // namespace

/* A runtime that lends more than the answer reads its later bytes as zero. */
TEST(caps_reply, a_whole_answer_fits_and_the_rest_is_zero)
{ std::uint8_t lent[sizeof(Options) + 8u];
  std::memset(lent, 0xEE, sizeof lent);
  CHECK(CapsReply(lent, sizeof lent).give_whole(&answer, sizeof answer));
  Options read = {};
  std::memcpy(&read, lent, sizeof read);
  CHECK_EQ(read.third, 0x33u);
  for (std::size_t at = sizeof(Options); at < sizeof lent; at++) CHECK_EQ(lent[at], 0u); }

/* A buffer one word short of a whole answer is refused, and not written. */
TEST(caps_reply, a_whole_answer_too_large_for_the_buffer_is_refused_untouched)
{ std::uint32_t lent[2] = {0xEEEEEEEEu, 0xEEEEEEEEu};
  CHECK(!CapsReply(lent, sizeof lent).give_whole(&answer, sizeof answer));
  CHECK_EQ(lent[0], 0xEEEEEEEEu);
  CHECK_EQ(lent[1], 0xEEEEEEEEu);
  std::uint32_t word = 7u;
  CHECK(!CapsReply(&word, 0u).give_whole(&answer.first, sizeof answer.first));
  CHECK_EQ(word, 7u); }

/* An older runtime's revision takes the fields it has. */
TEST(caps_reply, a_prefix_answer_gives_the_fields_the_buffer_has)
{ std::uint32_t older[2] = {0xEEEEEEEEu, 0xEEEEEEEEu};
  CHECK(CapsReply(older, sizeof older).give_prefix(&answer, sizeof answer));
  CHECK_EQ(older[0], 0x11u);
  CHECK_EQ(older[1], 0x22u);
  std::uint32_t newer[4] = {0xEEu, 0xEEu, 0xEEu, 0xEEu};
  CHECK(CapsReply(newer, sizeof newer).give_prefix(&answer, sizeof answer));
  CHECK_EQ(newer[2], 0x33u);
  CHECK_EQ(newer[3], 0u); }

TEST(caps_reply, none_is_all_zeroes_and_no_buffer_is_refused)
{ std::uint32_t lent[3] = {1u, 2u, 3u};
  CHECK(CapsReply(lent, sizeof lent).give_none());
  CHECK(lent[0] == 0u && lent[1] == 0u && lent[2] == 0u);
  CHECK(!CapsReply(nullptr, 16u).give_none());
  CHECK(!CapsReply(nullptr, 16u).give_whole(&answer, sizeof answer));
  CHECK(!CapsReply(nullptr, 16u).give_prefix(&answer, sizeof answer)); }
