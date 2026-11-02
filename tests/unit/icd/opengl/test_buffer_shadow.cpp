#include "core/support/status.hpp"
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/buffer_shadow.hpp"

#include <cstdint>
#include <cstring>

using av::MockPlatform;
using av::Status;
using av::glicd::BufferShadow;

namespace av::glicd {

struct BufferShadowProbe {
    static std::uint8_t *block(BufferShadow &shadow) { return shadow.block_; }
    static void set_block(BufferShadow &shadow, std::uint8_t *block) { shadow.block_ = block; }
    static void set_buffer(BufferShadow &shadow, std::uint32_t buffer) { shadow.buffer_ = buffer; }
    static void set_offset(BufferShadow &shadow, std::uint64_t offset) { shadow.offset_ = offset; }
    static void set_length(BufferShadow &shadow, std::uint64_t length) { shadow.length_ = length; }
};

}

namespace {

using Probe = av::glicd::BufferShadowProbe;

void pattern(std::uint8_t *bytes, std::size_t size, std::uint8_t seed)
{
    for (std::size_t at = 0u; at < size; at++) bytes[at] = static_cast<std::uint8_t>(at * 5u + seed);
}

}

TEST(buffer_shadow, holds_a_copy_of_its_own_of_the_bytes_it_is_given)
{ MockPlatform platform;
  BufferShadow shadow{platform};
  CHECK(shadow.invariants_hold());
  CHECK_EQ(shadow.buffer(), 0u);
  CHECK_EQ(shadow.length(), 0u);
  std::uint8_t bytes[32];
  pattern(bytes, sizeof bytes, 1u);
  CHECK_STATUS(shadow.hold(7u, 100u, sizeof bytes, bytes, 5u), Status::ok);
  CHECK(shadow.invariants_hold());
  CHECK_EQ(shadow.buffer(), 7u);
  CHECK_EQ(shadow.length(), sizeof bytes);
  CHECK_EQ(shadow.stamp(), 5u);
  const std::uint8_t first = bytes[0];
  bytes[0] = static_cast<std::uint8_t>(first + 1u);
  CHECK_EQ(shadow.at(100u, 6u)[0], first);
  CHECK_EQ(shadow.stamp(), 6u);
  CHECK_EQ(shadow.at(131u, 9u)[0], bytes[31]);
  CHECK_EQ(shadow.stamp(), 9u);
  CHECK_STATUS(shadow.hold(8u, 0u, 4u, bytes, 10u), Status::invalid_state);
  CHECK_EQ(shadow.buffer(), 7u); }

TEST(buffer_shadow, covers_only_its_own_buffer_inside_its_range)
{ MockPlatform platform;
  BufferShadow shadow{platform};
  CHECK(!shadow.covers(0u, 0u, 0u));
  std::uint8_t bytes[32] = {};
  CHECK_STATUS(shadow.hold(7u, 100u, sizeof bytes, bytes, 1u), Status::ok);
  constexpr struct {
      std::uint32_t buffer;
      std::uint64_t offset;
      std::uint64_t length;
      bool          covered;
  } cases[] = {
      {7u, 100u, 32u, true}, {7u, 100u, 0u, true},  {7u, 132u, 0u, true},  {7u, 131u, 1u, true},
      {7u, 99u, 1u, false},  {7u, 100u, 33u, false}, {7u, 120u, 13u, false}, {7u, 133u, 0u, false},
      {8u, 100u, 1u, false}, {0u, 100u, 1u, false},
  };
  for (const auto &row : cases) CHECK_EQ(shadow.covers(row.buffer, row.offset, row.length), row.covered); }

TEST(buffer_shadow, an_overwrite_takes_only_what_overlaps_it)
{ MockPlatform platform;
  BufferShadow shadow{platform};
  std::uint8_t zeros[16] = {};
  CHECK_STATUS(shadow.hold(7u, 100u, sizeof zeros, zeros, 1u), Status::ok);
  std::uint8_t source[8];
  pattern(source, sizeof source, 40u);
  shadow.overwrite(96u, 8u, source);
  shadow.overwrite(112u, 8u, source);
  shadow.overwrite(90u, 4u, source);
  shadow.overwrite(116u, 4u, source);
  shadow.overwrite(106u, 2u, source);
  const std::uint8_t *const copy = shadow.at(100u, 2u);
  CHECK(std::memcmp(copy, source + 4u, 4u) == 0);
  CHECK_EQ(copy[4], 0u);
  CHECK_EQ(copy[5], 0u);
  CHECK(std::memcmp(copy + 6u, source, 2u) == 0);
  CHECK_EQ(copy[8], 0u);
  CHECK_EQ(copy[11], 0u);
  CHECK(std::memcmp(copy + 12u, source, 4u) == 0); }

TEST(buffer_shadow, refuses_no_buffer_no_bytes_and_no_memory)
{ MockPlatform platform;
  BufferShadow shadow{platform};
  std::uint8_t bytes[4] = {};
  CHECK_STATUS(shadow.hold(0u, 0u, 4u, bytes, 1u), Status::invalid_parameter);
  CHECK_STATUS(shadow.hold(7u, 0u, 0u, bytes, 1u), Status::invalid_parameter);
  CHECK_STATUS(shadow.hold(7u, 0u, 4u, nullptr, 1u), Status::invalid_parameter);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(shadow.hold(7u, 0u, 4u, bytes, 1u), Status::no_memory);
  CHECK_EQ(shadow.buffer(), 0u);
  CHECK(shadow.invariants_hold()); }

TEST(buffer_shadow, reports_its_range_and_last_use)
{ MockPlatform platform;
  BufferShadow shadow{platform};
  std::uint8_t bytes[32] = {};
  CHECK_STATUS(shadow.hold(7u, 12u, sizeof bytes, bytes, 3u), Status::ok);
  char            text[128];
  av::StateWriter writer(text, sizeof text);
  shadow.report(writer, 2u);
  CHECK(std::strcmp(writer.dump(), "shadow/2 buffer=7 offset=12 length=32 stamp=3") == 0); }

TEST(buffer_shadow_invariants, fail_for_a_half_held_copy_or_a_range_that_wraps)
{ MockPlatform platform;
  BufferShadow shadow{platform};
  std::uint8_t bytes[8] = {};
  CHECK_STATUS(shadow.hold(7u, 0u, sizeof bytes, bytes, 1u), Status::ok);
  std::uint8_t *const held = Probe::block(shadow);
  Probe::set_block(shadow, nullptr);
  CHECK(!shadow.invariants_hold());
  Probe::set_block(shadow, held);
  Probe::set_length(shadow, 0u);
  CHECK(!shadow.invariants_hold());
  Probe::set_length(shadow, sizeof bytes);
  Probe::set_buffer(shadow, 0u);
  CHECK(!shadow.invariants_hold());
  Probe::set_buffer(shadow, 7u);
  Probe::set_offset(shadow, UINT64_MAX);
  CHECK(!shadow.invariants_hold());
  Probe::set_offset(shadow, 0u);
  CHECK(shadow.invariants_hold()); }
