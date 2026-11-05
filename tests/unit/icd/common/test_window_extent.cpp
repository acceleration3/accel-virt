#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/common/window_extent.hpp"

#include <cstdint>

using av::ScriptedWindowSurface;
using av::icd::WindowExtent;
using Look = av::icd::WindowExtent::Look;

namespace {

void *const dc = reinterpret_cast<void *>(0x100u);
void *const window = reinterpret_cast<void *>(0x200u);

}

TEST(window_extent, a_window_with_a_client_area_is_looked_at_with_its_size)
{ ScriptedWindowSurface surface;
  surface.add_dc(dc, window, 64u, 48u);
  WindowExtent extent(surface, window);
  CHECK(extent.look() == Look::resized);
  CHECK_EQ(extent.width(), 64u);
  CHECK_EQ(extent.height(), 48u); }

TEST(window_extent, a_window_the_size_a_presenter_holds_is_the_same)
{ ScriptedWindowSurface surface;
  surface.add_dc(dc, window, 64u, 48u);
  WindowExtent held(surface, window, 64u, 48u);
  CHECK(held.look() == Look::same);
  CHECK_EQ(held.width(), 64u);
  WindowExtent narrower(surface, window, 63u, 48u);
  CHECK(narrower.look() == Look::resized);
  WindowExtent shorter(surface, window, 64u, 47u);
  CHECK(shorter.look() == Look::resized);
  surface.resize(window, 80u, 60u);
  CHECK(held.look() == Look::resized);
  CHECK_EQ(held.width(), 80u);
  CHECK_EQ(held.height(), 60u); }

TEST(window_extent, a_window_without_a_client_area_is_empty)
{ ScriptedWindowSurface surface;
  surface.add_dc(dc, window, 64u, 48u);
  WindowExtent extent(surface, window, 64u, 48u);
  surface.resize(window, 0u, 48u);
  CHECK(extent.look() == Look::empty);
  surface.resize(window, 64u, 0u);
  CHECK(extent.look() == Look::empty);
  surface.resize(window, 0u, 0u);
  CHECK(extent.look() == Look::empty);
  surface.resize(window, 64u, 48u);
  CHECK(extent.look() == Look::same); }

TEST(window_extent, a_window_that_is_gone_or_was_never_there_is_gone_with_no_size)
{ ScriptedWindowSurface surface;
  surface.add_dc(dc, window, 64u, 48u);
  WindowExtent extent(surface, window);
  CHECK(extent.look() == Look::resized);
  surface.close_window(window);
  CHECK(extent.look() == Look::gone);
  CHECK_EQ(extent.width(), 0u);
  CHECK_EQ(extent.height(), 0u);
  WindowExtent none(surface, nullptr);
  CHECK(none.look() == Look::gone);
  WindowExtent unknown(surface, reinterpret_cast<void *>(0x1234u));
  CHECK(unknown.look() == Look::gone); }
