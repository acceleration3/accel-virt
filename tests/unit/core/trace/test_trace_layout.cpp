#include "core/trace/trace_layout.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::TraceLayout;

TEST(trace_layout, places_slots_and_frames_as_documented)
{ const TraceLayout layout(std::uint32_t{3});
  CHECK_EQ(layout.slots(), 3u);
  CHECK_EQ(layout.bytes(), 96u + 3u * 824u);
  CHECK_EQ(layout.slot(0u), 96u);
  CHECK_EQ(layout.slot(2u), 96u + 2u * 824u);
  CHECK_EQ(layout.frame(0u, 0u), 152u);
  CHECK_EQ(layout.frame(1u, 31u), 96u + 824u + 56u + 31u * 24u);
  CHECK_EQ(static_cast<std::uint32_t>(TraceLayout::slot_bytes), 824u);
  CHECK_EQ(TraceLayout::magic, 0x45434152544B5652ull); }

TEST(trace_layout, a_slot_count_is_clamped_to_256)
{ CHECK_EQ(TraceLayout(std::uint32_t{256}).slots(), 256u);
  CHECK_EQ(TraceLayout(std::uint32_t{257}).slots(), 256u);
  CHECK_EQ(TraceLayout(std::uint32_t{0xFFFFFFFFu}).slots(), 256u); }

TEST(trace_layout, a_size_holds_whole_slots_only)
{ CHECK_EQ(TraceLayout(std::size_t{0}).slots(), 0u);
  CHECK_EQ(TraceLayout(std::size_t{95}).slots(), 0u);
  CHECK_EQ(TraceLayout(std::size_t{96}).slots(), 0u);
  CHECK_EQ(TraceLayout(std::size_t{96 + 823}).slots(), 0u);
  CHECK_EQ(TraceLayout(std::size_t{96 + 824}).slots(), 1u);
  CHECK_EQ(TraceLayout(std::size_t{96 + 2 * 824 - 1}).slots(), 1u);
  CHECK_EQ(TraceLayout(std::size_t{0xFFFFFFFFFFFFFFFFull}).slots(), 256u); }
