/* The OpenGL ICD's scripted windows and threads are the oracles its tests judge
 * presentation and current contexts by, so their own rules are tested: a DC
 * answers its window or none, sizes follow the test, a closed window has no
 * size, a blit is kept or refused, and each thread keeps its own slot and
 * error. */
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_thread_slot.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "icd/opengl/thread_slot.hpp"
#include "icd/opengl/window_surface.hpp"

#include <cstdint>
#include <cstring>

using av::ScriptedThreadSlot;
using av::ScriptedWindowSurface;

namespace {

int dc_a = 0;
int dc_b = 0;
int memory_dc = 0;
int unknown_dc = 0;
int window_a = 0;

}  // namespace

TEST(scripted_window_surface, a_dc_answers_its_window_and_a_memory_or_unknown_dc_none)
{ ScriptedWindowSurface surface;
  surface.add_dc(&dc_a, &window_a, 64u, 48u);
  surface.add_dc(&dc_b, &window_a, 64u, 48u);
  surface.add_dc(&memory_dc, nullptr, 0u, 0u);
  av::glicd::WindowSurface &seam = surface;
  CHECK(seam.window_of(&dc_a) == &window_a);
  CHECK(seam.window_of(&dc_b) == &window_a);
  CHECK(seam.window_of(&memory_dc) == nullptr);
  CHECK(seam.window_of(&unknown_dc) == nullptr);
  CHECK(seam.window_of(nullptr) == nullptr); }

TEST(scripted_window_surface, sizes_follow_the_test_and_a_closed_window_has_none)
{ ScriptedWindowSurface surface;
  surface.add_dc(&dc_a, &window_a, 64u, 48u);
  std::uint32_t width = 0u;
  std::uint32_t height = 0u;
  CHECK(surface.client_size(&window_a, width, height));
  CHECK_EQ(width, 64u);
  CHECK_EQ(height, 48u);
  surface.resize(&window_a, 300u, 1u);
  CHECK(surface.client_size(&window_a, width, height));
  CHECK_EQ(width, 300u);
  CHECK_EQ(height, 1u);
  width = 7u;
  CHECK(!surface.client_size(&unknown_dc, width, height));
  CHECK_EQ(width, 7u);
  surface.close_window(&window_a);
  CHECK(!surface.client_size(&window_a, width, height));
  CHECK(surface.window_of(&dc_a) == nullptr); }

TEST(scripted_window_surface, keeps_each_blit_and_refuses_the_one_it_was_told_to)
{ ScriptedWindowSurface surface;
  surface.add_dc(&dc_a, &window_a, 2u, 1u);
  const std::uint8_t pixels[] = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u};
  CHECK_EQ(surface.blits(), 0u);
  CHECK(surface.blit(&dc_a, 2u, 1u, pixels));
  CHECK_EQ(surface.blits(), 1u);
  CHECK(surface.last_blit_dc() == &dc_a);
  CHECK_EQ(surface.last_blit_width(), 2u);
  CHECK_EQ(surface.last_blit_height(), 1u);
  CHECK(std::memcmp(surface.last_blit_bytes(), pixels, sizeof pixels) == 0);
  surface.refuse_next_blit();
  CHECK(!surface.blit(&dc_a, 2u, 1u, pixels));
  CHECK_EQ(surface.blits(), 1u);
  CHECK(surface.blit(&dc_a, 2u, 1u, pixels));
  CHECK_EQ(surface.blits(), 2u);
  /* A DC it does not know draws nowhere. */
  CHECK(!surface.blit(&unknown_dc, 2u, 1u, pixels));
  CHECK(!surface.blit(&memory_dc, 2u, 1u, pixels));
  CHECK_EQ(surface.blits(), 2u); }

TEST(scripted_window_surface, keeps_only_the_first_bytes_of_a_large_blit)
{ ScriptedWindowSurface surface;
  surface.add_dc(&dc_a, &window_a, 512u, 512u);
  static std::uint8_t pixels[512u * 512u * 4u];
  std::memset(pixels, 0x5a, sizeof pixels);
  CHECK(surface.blit(&dc_a, 512u, 512u, pixels));
  CHECK_EQ(surface.last_blit_bytes()[ScriptedWindowSurface::kept_bytes - 1u], 0x5au); }

TEST(scripted_thread_slot, each_thread_keeps_its_own_value_and_error)
{ ScriptedThreadSlot slot;
  av::glicd::ThreadSlot &seam = slot;
  int context = 0;
  CHECK_EQ(seam.thread_id(), ScriptedThreadSlot::first_thread);
  CHECK(seam.get() == nullptr);
  seam.set(&context);
  seam.set_last_error(0x2095u);
  slot.switch_to(2u);
  CHECK_EQ(seam.thread_id(), 2u);
  CHECK(seam.get() == nullptr);
  CHECK_EQ(slot.last_error(), 0u);
  seam.set_last_error(170u);
  slot.switch_to(1u);
  CHECK(seam.get() == &context);
  CHECK_EQ(slot.last_error(), 0x2095u);
  CHECK_EQ(slot.last_error(2u), 170u);
  slot.clear_last_error();
  CHECK_EQ(slot.last_error(), 0u);
  CHECK_EQ(slot.last_error(2u), 170u);
  seam.set(nullptr);
  CHECK(seam.get() == nullptr);
  slot.switch_to(ScriptedThreadSlot::max_threads);
  CHECK_EQ(seam.thread_id(), ScriptedThreadSlot::max_threads); }

TEST(scripted_thread_slot, a_thread_it_cannot_play_fails_the_test)
{ ScriptedThreadSlot slot;
  slot.switch_to(0u);
  CHECK(av::TestRegistry::instance().take_current_failure());
  slot.switch_to(ScriptedThreadSlot::max_threads + 1u);
  CHECK(av::TestRegistry::instance().take_current_failure());
  CHECK_EQ(slot.thread_id(), ScriptedThreadSlot::first_thread);
  CHECK_EQ(slot.last_error(0u), 0u); }
