/* Whether a frame put on a window now would be seen (feature 051, US2.3): a
 * window that is gone, minimized or empty in either dimension is one a swap
 * reads nothing back for. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/opengl/window_visibility.hpp"

#include <cstdint>

using av::ScriptedWindowSurface;
using av::glicd::WindowVisibility;

TEST(window_visibility, a_window_with_a_client_area_is_showing)
{ ScriptedWindowSurface surface;
  void *const dc = reinterpret_cast<void *>(0x100u);
  void *const window = reinterpret_cast<void *>(0x200u);
  surface.add_dc(dc, window, 64u, 48u);
  const WindowVisibility visible(surface, window);
  CHECK(visible.showing()); }

TEST(window_visibility, a_window_nobody_can_see_is_not)
{ ScriptedWindowSurface surface;
  void *const window = reinterpret_cast<void *>(0x200u);
  surface.add_dc(reinterpret_cast<void *>(0x100u), window, 64u, 48u);
  surface.resize(window, 0u, 48u);
  CHECK(!WindowVisibility(surface, window).showing());
  surface.resize(window, 64u, 0u);
  CHECK(!WindowVisibility(surface, window).showing());
  surface.resize(window, 0u, 0u);
  CHECK(!WindowVisibility(surface, window).showing());
  surface.resize(window, 64u, 48u);
  CHECK(WindowVisibility(surface, window).showing()); }

TEST(window_visibility, a_window_that_is_gone_or_was_never_there_is_not)
{ ScriptedWindowSurface surface;
  void *const window = reinterpret_cast<void *>(0x200u);
  surface.add_dc(reinterpret_cast<void *>(0x100u), window, 64u, 48u);
  surface.close_window(window);
  CHECK(!WindowVisibility(surface, window).showing());
  CHECK(!WindowVisibility(surface, nullptr).showing());
  CHECK(!WindowVisibility(surface, reinterpret_cast<void *>(0x1234u)).showing()); }
