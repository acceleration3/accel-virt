/* The host's error word as glGetError reads it (docs/protocol.md, "Error
 * word"): only a word with the host's marker is read, each kind whose count
 * moved is one error reported once, lowest kind first, and a count that
 * wrapped past 255 is still a move. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/reported_gl_errors.hpp"

#include <cstdint>

using av::glicd::ReportedGlErrors;

namespace {

constexpr std::uint32_t invalid_enum = 0x0500u, invalid_value = 0x0501u, invalid_operation = 0x0502u,
                        invalid_framebuffer_operation = 0x0506u;

std::uint64_t word(std::uint8_t enums, std::uint8_t values, std::uint8_t operations, std::uint8_t framebuffers)
{
    return ReportedGlErrors::published | std::uint64_t{enums} | std::uint64_t{values} << 8u |
           std::uint64_t{operations} << 16u | std::uint64_t{framebuffers} << 48u;
}

}  // namespace

TEST(reported_gl_errors, reads_only_a_word_the_host_marked)
{ ReportedGlErrors reported;
  CHECK(!reported.readable(0u));
  CHECK(!reported.readable(0x0000000000010203ull));
  CHECK(reported.readable(ReportedGlErrors::published));
  CHECK(reported.readable(word(1u, 0u, 0u, 0u))); }

TEST(reported_gl_errors, reports_each_kind_that_moved_once_lowest_first)
{ ReportedGlErrors reported;
  CHECK_EQ(reported.take(word(0u, 0u, 0u, 0u)), 0u);
  const std::uint64_t raised = word(0u, 2u, 1u, 3u);
  CHECK_EQ(reported.take(raised), invalid_value);
  CHECK_EQ(reported.take(raised), invalid_operation);
  CHECK_EQ(reported.take(raised), invalid_framebuffer_operation);
  CHECK_EQ(reported.take(raised), 0u);
  CHECK_EQ(reported.take(raised), 0u);
  /* More of a kind already reported is one error again, not one per count. */
  CHECK_EQ(reported.take(word(1u, 7u, 1u, 3u)), invalid_enum);
  CHECK_EQ(reported.take(word(1u, 7u, 1u, 3u)), invalid_value);
  CHECK_EQ(reported.take(word(1u, 7u, 1u, 3u)), 0u); }

TEST(reported_gl_errors, a_count_that_wrapped_is_still_a_move)
{ ReportedGlErrors reported;
  CHECK_EQ(reported.take(word(255u, 0u, 0u, 0u)), invalid_enum);
  CHECK_EQ(reported.take(word(0u, 0u, 0u, 0u)), invalid_enum);
  CHECK_EQ(reported.take(word(0u, 0u, 0u, 0u)), 0u); }
