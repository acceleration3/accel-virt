#include "executor/opencl/cl_binary_digest.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ClBinaryDigest;

TEST(cl_binary_digest, the_published_fnv1a_64_vectors)
{ { const ClBinaryDigest nothing;
    CHECK_EQ(nothing.value(), 0xCBF29CE484222325ull); }
  { ClBinaryDigest one;
    one.add("a", 1u);
    CHECK_EQ(one.value(), 0xAF63DC4C8601EC8Cull); }
  { ClBinaryDigest word;
    word.add("foobar", 6u);
    CHECK_EQ(word.value(), 0x85944171F73967E8ull); }
  { ClBinaryDigest pieces;
    pieces.add("foo", 3u);
    pieces.add(nullptr, 5u);
    pieces.add("bar", 3u);
    CHECK_EQ(pieces.value(), 0x85944171F73967E8ull); } }

TEST(cl_binary_digest, text_is_digested_with_its_end)
{ ClBinaryDigest joined;
  joined.add_text("ab");
  joined.add_text("c");
  ClBinaryDigest moved;
  moved.add_text("a");
  moved.add_text("bc");
  CHECK(joined.value() != moved.value());
  ClBinaryDigest bytes;
  bytes.add("a", 2u);
  ClBinaryDigest text;
  text.add_text("a");
  CHECK_EQ(text.value(), bytes.value());
  ClBinaryDigest none;
  none.add_text(nullptr);
  ClBinaryDigest empty;
  empty.add_text("");
  CHECK_EQ(none.value(), empty.value());
  CHECK(none.value() != ClBinaryDigest().value()); }
