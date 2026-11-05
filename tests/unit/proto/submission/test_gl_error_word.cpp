#include "harness/framework/test_registry.hpp"
#include "proto/submission/gl_error_word.hpp"

#include <cstdint>

using av::GlErrorWord;

TEST(gl_error_word, starts_as_the_bare_published_marker)
{ const GlErrorWord word;
  CHECK_EQ(word.word(), 0x8000000000000000ull);
  CHECK(word.published());
  for (std::uint32_t kind = 0u; kind < GlErrorWord::kinds; kind++) CHECK_EQ(word.count(kind), 0u); }

TEST(gl_error_word, a_word_read_from_the_host_is_published_only_when_the_host_marked_it)
{ CHECK(!GlErrorWord(0u).published());
  CHECK(!GlErrorWord(0x0000000000010203ull).published());
  CHECK(GlErrorWord(0x8000000000010203ull).published());
  CHECK_EQ(GlErrorWord(0x8000000000010203ull).word(), 0x8000000000010203ull);
  CHECK_EQ(GlErrorWord(0x8000000000010203ull).count(0u), 3u);
  CHECK_EQ(GlErrorWord(0x8000000000010203ull).count(1u), 2u);
  CHECK_EQ(GlErrorWord(0x8000000000010203ull).count(2u), 1u); }

TEST(gl_error_word, counts_each_kind_in_its_own_byte)
{ GlErrorWord word;
  word.record(0x0500u);
  word.record(0x0502u);
  word.record(0x0502u);
  word.record(0x0506u);
  CHECK_EQ(word.word(), 0x8001000000020001ull);
  CHECK_EQ(word.count(0u), 1u);
  CHECK_EQ(word.count(2u), 2u);
  CHECK_EQ(word.count(6u), 1u);
  word.record(0x0507u);
  word.record(0u);
  word.record(0x04FFu);
  word.record(0x8031u);
  CHECK_EQ(word.word(), 0x8001000000020001ull);
  CHECK_EQ(word.count(GlErrorWord::kinds), 0u);
  CHECK_EQ(word.count(0xFFFFFFFFu), 0u); }

TEST(gl_error_word, a_count_wraps_without_touching_its_neighbours)
{ GlErrorWord word;
  for (int n = 0; n < 256; n++) word.record(0x0501u);
  word.record(0x0500u);
  CHECK_EQ(word.word(), 0x8000000000000001ull);
  CHECK_EQ(word.count(1u), 0u); }
