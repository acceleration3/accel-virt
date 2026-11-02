#include "core/support/status.hpp"
#include "core/trace/event_ring.hpp"
#include "core/trace/event_ring_layout.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstddef>
#include <cstdint>

using av::EventRing;
using av::EventRingLayout;
using av::Status;

TEST(event_ring_layout, a_header_then_each_slot_s_header_and_events)
{ CHECK_EQ(EventRingLayout(1u, 1u).bytes(), 32u + 16u + 16u);
  CHECK_EQ(EventRingLayout(3u, 4u).bytes(), 32u + 3u * (16u + 4u * 16u));
  CHECK_EQ(EventRingLayout(0u, 8u).bytes(), 32u);
  CHECK_EQ(EventRingLayout(2u, 0u).bytes(), 32u + 2u * 16u); }

TEST(event_ring_layout, the_largest_ring_does_not_wrap)
{ const std::size_t expected = 32u + std::size_t{256} * (16u + (std::size_t{1} << 22u) * 16u);
  CHECK(EventRingLayout(256u, 1u << 22u).bytes() == expected);
  CHECK(EventRingLayout(256u, 1u << 22u).bytes() > (std::size_t{1} << 32u)); }

TEST(event_ring_layout, is_the_size_a_ring_asks_its_region_for)
{ alignas(8) std::uint8_t region[32u + 2u * (16u + 3u * 16u)] = {};
  const std::size_t bytes = EventRingLayout(2u, 3u).bytes();
  CHECK_EQ(bytes, sizeof region);
  EventRing short_ring;
  CHECK_STATUS(short_ring.initialize(region, bytes - 1u, 2u, 3u), Status::invalid_parameter);
  CHECK(!short_ring.recording());
  EventRing ring;
  CHECK_STATUS(ring.initialize(region, bytes, 2u, 3u), Status::ok);
  CHECK(ring.recording()); }
