#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

namespace av {

struct DisplayModeSetProbe {
    static void set_count(DisplayModeSet &set, std::uint32_t count) { set.count_ = count; }
    static void set_preferred(DisplayModeSet &set, std::uint32_t preferred) { set.preferred_ = preferred; }
    static void set_refresh(DisplayModeSet &set, std::uint32_t millihz) { set.refresh_millihz_ = millihz; }
    static void set_width(DisplayModeSet &set, std::uint32_t index, std::uint32_t width) { set.widths_[index] = width; }
    static void set_height(DisplayModeSet &set, std::uint32_t index, std::uint32_t height) { set.heights_[index] = height; }
};

}

using av::DisplayModeSet;
using av::DisplayModeSetProbe;
using av::Status;

namespace {
std::size_t response(std::uint8_t (&bytes)[DisplayModeSet::max_encoded_size + 16u], std::uint32_t type,
                     std::uint32_t count, std::uint32_t preferred, const std::uint32_t *edges)
{
    std::memset(bytes, 0, sizeof bytes);
    std::memcpy(bytes, &type, sizeof type);
    std::memcpy(bytes + 24u, &count, sizeof count);
    std::memcpy(bytes + 28u, &preferred, sizeof preferred);
    const std::uint32_t written = count <= 2u ? count : 2u;
    std::memcpy(bytes + 32u, edges, written * 8u);
    return 32u + written * 8u;
}

}

TEST(display_mode_set, an_unusual_resolution_round_trips_and_stays_preferred)
{ DisplayModeSet set;
  CHECK_STATUS(set.add(80u, 80u), Status::ok);
  CHECK_STATUS(set.add(2560u, 1440u), Status::ok);
  CHECK_STATUS(set.add(1u, 16384u), Status::ok);
  CHECK_STATUS(set.prefer(1u), Status::ok);
  std::uint8_t bytes[DisplayModeSet::max_encoded_size] = {};
  av::WireWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(set.encode(writer), Status::ok);
  CHECK_EQ(writer.offset(), set.encoded_size());
  CHECK_EQ(set.encoded_size(), 32u + 3u * 8u + 4u);
  DisplayModeSet read;
  CHECK_STATUS(read.decode(bytes, writer.offset()), Status::ok);
  CHECK(read.same_as(set));
  CHECK_EQ(read.width(0u), 80u);
  CHECK_EQ(read.height(0u), 80u);
  CHECK_EQ(read.width(read.preferred()), 2560u);
  CHECK_EQ(read.height(read.preferred()), 1440u);
  CHECK(read.invariants_hold()); }

TEST(display_mode_set, a_mode_is_offered_once_and_the_set_is_bounded)
{ DisplayModeSet set;
  CHECK_STATUS(set.add(1920u, 1080u), Status::ok);
  CHECK_STATUS(set.add(1920u, 1080u), Status::ok);
  CHECK_EQ(set.count(), 1u);
  CHECK_STATUS(set.add(0u, 600u), Status::invalid_parameter);
  CHECK_STATUS(set.add(800u, 0u), Status::invalid_parameter);
  CHECK_STATUS(set.add(16385u, 600u), Status::invalid_parameter);
  CHECK_STATUS(set.add(800u, 16385u), Status::invalid_parameter);
  for (std::uint32_t width = 100u; set.count() < DisplayModeSet::max_modes; width++)
      CHECK_STATUS(set.add(width, 100u), Status::ok);
  CHECK_STATUS(set.add(99u, 99u), Status::no_space);
  CHECK_STATUS(set.prefer(DisplayModeSet::max_modes), Status::invalid_parameter);
  CHECK_EQ(set.width(DisplayModeSet::max_modes), 0u);
  CHECK(set.invariants_hold());
  set.clear();
  std::uint8_t   bytes[DisplayModeSet::max_encoded_size] = {};
  av::WireWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(set.encode(writer), Status::invalid_state); }

TEST(display_mode_set, a_response_that_breaks_a_rule_is_refused_and_changes_nothing)
{ DisplayModeSet kept;
  CHECK_STATUS(kept.add(1280u, 800u), Status::ok);
  const std::uint32_t good[] = {1920u, 1080u, 80u, 80u};
  const std::uint32_t repeated[] = {1920u, 1080u, 1920u, 1080u};
  const std::uint32_t zero[] = {1920u, 0u};
  const std::uint32_t huge[] = {16385u, 1080u};
  std::uint8_t bytes[DisplayModeSet::max_encoded_size + 16u] = {};
  const struct {
      std::uint32_t        type, count, preferred;
      const std::uint32_t *edges;
      std::size_t          trim;
  } refused[] = {
      {0x1101u, 2u, 0u, good, 0u},
      {DisplayModeSet::response_type, 0u, 0u, good, 0u},
      {DisplayModeSet::response_type, 2u, 2u, good, 0u},
      {DisplayModeSet::response_type, 2u, 0u, good, 1u},
      {DisplayModeSet::response_type, 2u, 0u, repeated, 0u},
      {DisplayModeSet::response_type, 1u, 0u, zero, 0u},
      {DisplayModeSet::response_type, 1u, 0u, huge, 0u},
      {DisplayModeSet::response_type, DisplayModeSet::max_modes + 1u, 0u, good, 0u},
  };
  for (const auto &row : refused) {
      const std::size_t length = response(bytes, row.type, row.count, row.preferred, row.edges) - row.trim;
      CHECK_STATUS(kept.decode(bytes, length), Status::malformed);
      CHECK_EQ(kept.count(), 1u);
      CHECK_EQ(kept.width(0u), 1280u);
  }
  CHECK_STATUS(kept.decode(bytes, 31u), Status::malformed);
  CHECK_STATUS(kept.decode(nullptr, 64u), Status::malformed);
  const std::size_t length = response(bytes, DisplayModeSet::response_type, 2u, 1u, good);
  CHECK_STATUS(kept.decode(bytes, length), Status::ok);
  CHECK_EQ(kept.count(), 2u);
  CHECK_EQ(kept.width(kept.preferred()), 80u);
  CHECK(kept.invariants_hold()); }

TEST(display_mode_set, the_refresh_follows_the_modes_and_a_set_without_one_is_60_hz)
{ DisplayModeSet set;
  CHECK_EQ(set.refresh_millihz(), 60000u);
  CHECK_STATUS(set.add(2560u, 1440u), Status::ok);
  CHECK_STATUS(set.set_refresh(359980u), Status::ok);
  CHECK_STATUS(set.set_refresh(999u), Status::invalid_parameter);
  CHECK_STATUS(set.set_refresh(1000001u), Status::invalid_parameter);
  CHECK_EQ(set.refresh_millihz(), 359980u);
  std::uint8_t   bytes[DisplayModeSet::max_encoded_size] = {};
  av::WireWriter writer(bytes, sizeof bytes);
  CHECK_STATUS(set.encode(writer), Status::ok);
  DisplayModeSet read;
  CHECK_STATUS(read.decode(bytes, writer.offset()), Status::ok);
  CHECK_EQ(read.refresh_millihz(), 359980u);
  CHECK(read.same_as(set));
  DisplayModeSet old;
  CHECK_STATUS(old.decode(bytes, writer.offset() - DisplayModeSet::refresh_size), Status::ok);
  CHECK_EQ(old.refresh_millihz(), 60000u);
  std::memset(bytes + writer.offset() - DisplayModeSet::refresh_size, 0, DisplayModeSet::refresh_size);
  CHECK_STATUS(old.decode(bytes, writer.offset()), Status::ok);
  CHECK_EQ(old.refresh_millihz(), 60000u);
  const std::uint32_t slow = 500u;
  std::memcpy(bytes + writer.offset() - DisplayModeSet::refresh_size, &slow, sizeof slow);
  CHECK_STATUS(old.decode(bytes, writer.offset()), Status::malformed);
  CHECK(old.invariants_hold()); }

TEST(display_mode_set, a_mode_sharing_one_edge_with_another_is_a_different_mode)
{ DisplayModeSet set;
  CHECK_STATUS(set.add(1920u, 1080u), Status::ok);
  CHECK_STATUS(set.add(1920u, 720u), Status::ok);
  CHECK_STATUS(set.add(1280u, 1080u), Status::ok);
  CHECK_EQ(set.count(), 3u);
  CHECK_EQ(set.height(1u), 720u);
  CHECK_EQ(set.height(3u), 0u);
  CHECK_EQ(set.width(3u), 0u);
  CHECK(set.invariants_hold()); }

TEST(display_mode_set, two_sets_are_the_same_only_when_every_part_agrees)
{ DisplayModeSet one;
  CHECK_STATUS(one.add(1920u, 1080u), Status::ok);
  CHECK_STATUS(one.add(1280u, 720u), Status::ok);
  DisplayModeSet other = one;
  CHECK(one.same_as(other));
  CHECK_STATUS(other.add(800u, 600u), Status::ok);
  CHECK(!one.same_as(other));
  other = one;
  CHECK_STATUS(other.prefer(1u), Status::ok);
  CHECK(!one.same_as(other));
  other = one;
  CHECK_STATUS(other.set_refresh(144000u), Status::ok);
  CHECK(!one.same_as(other));
  DisplayModeSet wider;
  CHECK_STATUS(wider.add(1921u, 1080u), Status::ok);
  CHECK_STATUS(wider.add(1280u, 720u), Status::ok);
  CHECK(!one.same_as(wider));
  DisplayModeSet taller;
  CHECK_STATUS(taller.add(1920u, 1081u), Status::ok);
  CHECK_STATUS(taller.add(1280u, 720u), Status::ok);
  CHECK(!one.same_as(taller)); }

TEST(display_mode_set, the_invariant_catches_each_way_a_set_can_be_broken)
{ DisplayModeSet whole;
  CHECK_STATUS(whole.add(1920u, 1080u), Status::ok);
  CHECK_STATUS(whole.add(1280u, 720u), Status::ok);
  CHECK(whole.invariants_hold());

  DisplayModeSet more = whole;
  DisplayModeSetProbe::set_count(more, DisplayModeSet::max_modes + 1u);
  CHECK(!more.invariants_hold());

  DisplayModeSet empty;
  DisplayModeSetProbe::set_preferred(empty, 1u);
  CHECK(!empty.invariants_hold());
  DisplayModeSet beyond = whole;
  DisplayModeSetProbe::set_preferred(beyond, 2u);
  CHECK(!beyond.invariants_hold());

  DisplayModeSet slow = whole;
  DisplayModeSetProbe::set_refresh(slow, DisplayModeSet::least_refresh_millihz - 1u);
  CHECK(!slow.invariants_hold());
  DisplayModeSet fast = whole;
  DisplayModeSetProbe::set_refresh(fast, DisplayModeSet::most_refresh_millihz + 1u);
  CHECK(!fast.invariants_hold());

  DisplayModeSet no_width = whole;
  DisplayModeSetProbe::set_width(no_width, 1u, 0u);
  CHECK(!no_width.invariants_hold());
  DisplayModeSet no_height = whole;
  DisplayModeSetProbe::set_height(no_height, 1u, 0u);
  CHECK(!no_height.invariants_hold());

  DisplayModeSet wide = whole;
  DisplayModeSetProbe::set_width(wide, 1u, DisplayModeSet::max_dimension + 1u);
  CHECK(!wide.invariants_hold());
  DisplayModeSet tall = whole;
  DisplayModeSetProbe::set_height(tall, 1u, DisplayModeSet::max_dimension + 1u);
  CHECK(!tall.invariants_hold());

  DisplayModeSet repeated = whole;
  DisplayModeSetProbe::set_width(repeated, 1u, 1920u);
  DisplayModeSetProbe::set_height(repeated, 1u, 1080u);
  CHECK(!repeated.invariants_hold());
  DisplayModeSet sharing = whole;
  DisplayModeSetProbe::set_width(sharing, 1u, 1920u);
  CHECK(sharing.invariants_hold()); }
