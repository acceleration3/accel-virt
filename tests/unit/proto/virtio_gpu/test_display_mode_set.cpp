/* DisplayModeSet: any resolution a display could have, not only the standard
 * ones, round-trips between the device's encode and the driver's decode; the
 * preferred mode is one of the set; and a response that breaks a rule leaves
 * the last good set as it was. */
#include "harness/framework/test_registry.hpp"
#include "proto/virtio_gpu/display_mode_set.hpp"
#include "proto/wire/wire_writer.hpp"

#include <cstdint>
#include <cstring>

using av::DisplayModeSet;
using av::Status;

namespace {

/* A response of `count` modes written by hand, as a hostile device would. */
std::size_t response(std::uint8_t (&bytes)[DisplayModeSet::max_encoded_size + 16u], std::uint32_t type,
                     std::uint32_t count, std::uint32_t preferred, const std::uint32_t *edges)
{
    std::memset(bytes, 0, sizeof bytes);
    std::memcpy(bytes, &type, sizeof type);
    std::memcpy(bytes + 24u, &count, sizeof count);
    std::memcpy(bytes + 28u, &preferred, sizeof preferred);
    /* Every edge array below holds at most two modes. */
    const std::uint32_t written = count <= 2u ? count : 2u;
    std::memcpy(bytes + 32u, edges, written * 8u);
    return 32u + written * 8u;
}

}  // namespace

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
  /* An empty set has nothing to say. */
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
      {0x1101u, 2u, 0u, good, 0u},                           /* another response's type */
      {DisplayModeSet::response_type, 0u, 0u, good, 0u},     /* no mode */
      {DisplayModeSet::response_type, 2u, 2u, good, 0u},     /* preferred past the count */
      {DisplayModeSet::response_type, 2u, 0u, good, 1u},     /* the last mode cut short */
      {DisplayModeSet::response_type, 2u, 0u, repeated, 0u}, /* a mode twice */
      {DisplayModeSet::response_type, 1u, 0u, zero, 0u},     /* an edge of 0 */
      {DisplayModeSet::response_type, 1u, 0u, huge, 0u},     /* an edge past the bound */
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
  /* A device older than the refresh ends its answer after the modes; a zero is as good as none. */
  DisplayModeSet old;
  CHECK_STATUS(old.decode(bytes, writer.offset() - DisplayModeSet::refresh_size), Status::ok);
  CHECK_EQ(old.refresh_millihz(), 60000u);
  std::memset(bytes + writer.offset() - DisplayModeSet::refresh_size, 0, DisplayModeSet::refresh_size);
  CHECK_STATUS(old.decode(bytes, writer.offset()), Status::ok);
  CHECK_EQ(old.refresh_millihz(), 60000u);
  /* A rate out of range refuses the answer. */
  const std::uint32_t slow = 500u;
  std::memcpy(bytes + writer.offset() - DisplayModeSet::refresh_size, &slow, sizeof slow);
  CHECK_STATUS(old.decode(bytes, writer.offset()), Status::malformed);
  CHECK(old.invariants_hold()); }
