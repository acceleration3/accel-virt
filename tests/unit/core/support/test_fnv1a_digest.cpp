#include "core/support/fnv1a_digest.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::Fnv1aDigest;

TEST(fnv1a_digest, the_published_fnv1a_64_vectors)
{ { const Fnv1aDigest nothing;
    CHECK_EQ(nothing.value(), 0xCBF29CE484222325ull); }
  { Fnv1aDigest one;
    one.add("a", 1u);
    CHECK_EQ(one.value(), 0xAF63DC4C8601EC8Cull); }
  { Fnv1aDigest word;
    word.add("foobar", 6u);
    CHECK_EQ(word.value(), 0x85944171F73967E8ull); }
  { Fnv1aDigest pieces;
    pieces.add("foo", 3u);
    pieces.add(nullptr, 5u);
    pieces.add("bar", 3u);
    CHECK_EQ(pieces.value(), 0x85944171F73967E8ull); } }

TEST(fnv1a_digest, text_is_digested_with_its_end)
{ Fnv1aDigest joined;
  joined.add_text("ab");
  joined.add_text("c");
  Fnv1aDigest moved;
  moved.add_text("a");
  moved.add_text("bc");
  CHECK(joined.value() != moved.value());
  Fnv1aDigest bytes;
  bytes.add("a", 2u);
  Fnv1aDigest text;
  text.add_text("a");
  CHECK_EQ(text.value(), bytes.value());
  Fnv1aDigest none;
  none.add_text(nullptr);
  Fnv1aDigest empty;
  empty.add_text("");
  CHECK_EQ(none.value(), empty.value());
  CHECK(none.value() != Fnv1aDigest().value()); }

TEST(fnv1a_digest, a_word_is_folded_whole_and_a_salt_moves_the_start)
{ Fnv1aDigest word;
  word.add_word(7u);
  CHECK_EQ(word.value(), (0xCBF29CE484222325ull ^ 7u) * 0x00000100000001B3ull);
  const Fnv1aDigest salted(0x55u);
  CHECK_EQ(salted.value(), 0xCBF29CE484222325ull ^ 0x55u);
  CHECK(salted.value() != Fnv1aDigest().value()); }
