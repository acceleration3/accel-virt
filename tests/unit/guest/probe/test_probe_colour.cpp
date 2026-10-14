/* The colour every picture of the probes is judged against: each channel
 * within one, at both edges of that, for pixels of any spacing, with the bytes
 * past the channels never looked at. */
#include "guest/probe/probe_colour.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::ProbeColour;

namespace {

constexpr std::uint8_t solid[4] = {64u, 128u, 191u, 255u};

}  // namespace

TEST(probe_colour, a_channel_one_off_either_way_is_the_colour_and_two_off_is_not)
{ const ProbeColour colour(solid, 4u);
  for (std::uint32_t channel = 0u; channel < 3u; channel++) {
      std::uint8_t pixel[4] = {solid[0], solid[1], solid[2], solid[3]};
      CHECK_EQ(colour.matched(pixel, 1u, 4u), 1u);
      pixel[channel] = static_cast<std::uint8_t>(solid[channel] + 1u);
      CHECK_EQ(colour.matched(pixel, 1u, 4u), 1u);
      pixel[channel] = static_cast<std::uint8_t>(solid[channel] - 1u);
      CHECK_EQ(colour.matched(pixel, 1u, 4u), 1u);
      pixel[channel] = static_cast<std::uint8_t>(solid[channel] + 2u);
      CHECK_EQ(colour.matched(pixel, 1u, 4u), 0u);
      pixel[channel] = static_cast<std::uint8_t>(solid[channel] - 2u);
      CHECK_EQ(colour.matched(pixel, 1u, 4u), 0u);
  }
  /* The alpha of 255 is judged too when it is a channel: 253 is not it. */
  const std::uint8_t faded[4] = {solid[0], solid[1], solid[2], 253u};
  CHECK_EQ(colour.matched(faded, 1u, 4u), 0u); }

TEST(probe_colour, bytes_past_its_channels_are_not_looked_at)
{ const ProbeColour colour(solid, 3u);
  const std::uint8_t desktop_alpha[4] = {solid[0], solid[1], solid[2], 0u};
  CHECK_EQ(colour.matched(desktop_alpha, 1u, 4u), 1u);
  /* More channels than a pixel has are as many as it has. */
  const std::uint8_t five[5] = {solid[0], solid[1], solid[2], solid[3], 9u};
  const ProbeColour  wide(five, 5u);
  const std::uint8_t pixel[8] = {solid[0], solid[1], solid[2], solid[3], 0u, 0u, 0u, 0u};
  CHECK_EQ(wide.matched(pixel, 1u, 4u), 1u); }

TEST(probe_colour, pixels_are_counted_one_by_one_at_their_spacing)
{ const ProbeColour colour(solid, 3u);
  /* Three-byte pixels: the second is off by two in blue. */
  const std::uint8_t packed[9] = {64u, 128u, 191u, 64u, 128u, 193u, 63u, 127u, 192u};
  CHECK_EQ(colour.matched(packed, 3u, 3u), 2u);
  CHECK_EQ(colour.matched(packed, 1u, 3u), 1u);
  CHECK_EQ(colour.matched(packed, 0u, 3u), 0u);
  /* The same bytes four apart: the second pixel starts at the stray 64. */
  const std::uint8_t spaced[8] = {64u, 128u, 191u, 0u, 64u, 128u, 191u, 7u};
  CHECK_EQ(colour.matched(spaced, 2u, 4u), 2u); }
