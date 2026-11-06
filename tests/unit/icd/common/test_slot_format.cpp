#include "harness/framework/test_registry.hpp"
#include "icd/common/slot_format.hpp"
#include "icd/common/window_chains.hpp"

#include <cstdint>

using av::icd::SlotFormat;
using av::icd::SlotLayout;

namespace {

constexpr std::uint32_t vk_b8g8r8a8_unorm = 44u;
constexpr std::uint32_t vk_b8g8r8a8_srgb = 50u;
constexpr std::uint32_t vk_r8g8b8a8_unorm = 37u;
constexpr std::uint32_t vk_r8g8b8a8_srgb = 43u;
constexpr std::uint32_t gl_rgba8 = 0x8058u;
constexpr std::uint32_t gl_srgb8_alpha8 = 0x8C43u;

}

TEST(slot_format, a_layout_names_the_dxgi_format_and_the_vulkan_pair_of_its_byte_order)
{ const SlotFormat bgra(SlotLayout::bgra_top_down);
  CHECK(bgra.bgra());
  CHECK_EQ(bgra.dxgi(), 87u);
  CHECK_EQ(bgra.vk_format(), vk_b8g8r8a8_unorm);
  CHECK_EQ(bgra.vk_srgb(), vk_b8g8r8a8_srgb);
  const SlotFormat rgba(SlotLayout::rgba_top_down);
  CHECK(!rgba.bgra());
  CHECK_EQ(rgba.dxgi(), 28u);
  CHECK_EQ(rgba.vk_format(), vk_r8g8b8a8_unorm);
  CHECK_EQ(rgba.vk_srgb(), vk_r8g8b8a8_srgb);
  CHECK_EQ(rgba.bind_flags(), 0x28u);
  CHECK_EQ(bgra.bind_flags(), 0x28u); }

TEST(slot_format, only_the_bottom_up_rgba_layout_has_gl_storage)
{ const SlotFormat bottom_up(SlotLayout::rgba_bottom_up);
  CHECK_EQ(bottom_up.gl_storage(false), gl_rgba8);
  CHECK_EQ(bottom_up.gl_storage(true), gl_srgb8_alpha8);
  CHECK_EQ(SlotFormat(SlotLayout::rgba_top_down).gl_storage(false), 0u);
  CHECK_EQ(SlotFormat(SlotLayout::bgra_top_down).gl_storage(true), 0u); }

TEST(slot_format, a_vulkan_format_selects_the_layout_that_holds_it_and_anything_else_changes_nothing)
{ SlotFormat format(SlotLayout::rgba_top_down);
  CHECK(format.take_vk(vk_b8g8r8a8_srgb));
  CHECK(format.layout() == SlotLayout::bgra_top_down);
  CHECK(format.take_vk(vk_r8g8b8a8_unorm));
  CHECK(format.layout() == SlotLayout::rgba_top_down);
  CHECK(format.take_vk(vk_b8g8r8a8_unorm));
  CHECK(format.layout() == SlotLayout::bgra_top_down);
  CHECK(!format.take_vk(0u));
  CHECK(!format.take_vk(97u));
  CHECK(format.layout() == SlotLayout::bgra_top_down); }
