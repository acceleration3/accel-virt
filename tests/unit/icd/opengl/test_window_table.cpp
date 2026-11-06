#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/icd/scripted_window_surface.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/window_table.hpp"

#include <cstring>

using av::MockPlatform;
using av::Status;
using av::glicd::WindowTable;

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }
};

}

namespace av::glicd {

struct WindowTableProbe {
    static void corrupt_table(WindowTable &table) { KeyedSlotsProbe::corrupt_count(table.formats_, 99u); }
    static void plant(WindowTable &table, void *window, std::int32_t format)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(table.formats_.insert(reinterpret_cast<std::uintptr_t>(window), format, slot));
    }
};

}

TEST(window_table, a_window_takes_one_format_and_keeps_it)
{ MockPlatform platform;
  WindowTable  table(platform);
  int first = 0;
  int second = 0;
  CHECK_EQ(table.format_of(&first), 0);
  CHECK_STATUS(table.set_format(&first, 3), Status::ok);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.format_of(&first), 3);
  CHECK_STATUS(table.set_format(&first, 4), Status::conflict);
  CHECK_STATUS(table.set_format(&first, 3), Status::conflict);
  CHECK_EQ(table.format_of(&first), 3);
  CHECK_STATUS(table.set_format(&second, 1), Status::ok);
  CHECK_EQ(table.held(), 2u);
  CHECK(table.invariants_hold()); }

TEST(window_table, refuses_no_window_a_format_below_one_and_one_with_no_memory)
{ MockPlatform platform;
  WindowTable  table(platform);
  int window = 0;
  CHECK_STATUS(table.set_format(nullptr, 1), Status::invalid_parameter);
  CHECK_STATUS(table.set_format(&window, 0), Status::invalid_parameter);
  CHECK_STATUS(table.set_format(&window, -1), Status::invalid_parameter);
  CHECK_EQ(table.held(), 0u);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(table.set_format(&window, 1), Status::no_memory);
  CHECK_EQ(table.format_of(&window), 0);
  CHECK(table.invariants_hold()); }

TEST(window_table, keeps_windows_past_the_old_bound)
{ MockPlatform platform;
  WindowTable  table(platform);
  constexpr std::uint32_t many = 2000u;
  static int windows[many] = {};
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK_STATUS(table.set_format(&windows[index], 1 + static_cast<int>(index % 5u)), Status::ok);
  CHECK_EQ(table.held(), many);
  for (std::uint32_t index = 0u; index < many; index++)
      CHECK_EQ(table.format_of(&windows[index]), 1 + static_cast<int>(index % 5u));
  CHECK(table.invariants_hold()); }

TEST(window_table, drops_the_rows_of_windows_that_are_gone)
{ MockPlatform platform;
  WindowTable  table(platform);
  av::ScriptedWindowSurface surface;
  int dcs[3] = {};
  int windows[3] = {};
  for (int index = 0; index < 3; index++) {
    surface.add_dc(&dcs[index], &windows[index], 8u, 8u);
    CHECK_STATUS(table.set_format(&windows[index], index + 1), Status::ok);
  }
  table.drop_gone(surface);
  CHECK_EQ(table.held(), 3u);
  surface.close_window(&windows[0]);
  surface.close_window(&windows[2]);
  table.drop_gone(surface);
  CHECK(table.invariants_hold());
  CHECK_EQ(table.held(), 1u);
  CHECK_EQ(table.format_of(&windows[0]), 0);
  CHECK_EQ(table.format_of(&windows[1]), 2);
  CHECK_EQ(table.format_of(&windows[2]), 0);
  CHECK_STATUS(table.set_format(&windows[0], 5), Status::ok);
  CHECK_EQ(table.format_of(&windows[0]), 5); }

TEST(window_table, reports_each_window_and_its_format)
{ MockPlatform platform;
  WindowTable  table(platform);
  CHECK_STATUS(table.set_format(reinterpret_cast<void *>(0x1234u), 5), Status::ok);
  char bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  table.report(writer);
  CHECK(std::strcmp(writer.dump(),
                    "windows/0 held=1\nwindows/0/window/0 hwnd=0x0000000000001234 format=5") == 0); }

TEST(window_table_invariants, fail_for_rows_no_set_writes)
{ MockPlatform platform;
  int window = 0;
  WindowTable empty(platform);
  av::glicd::WindowTableProbe::plant(empty, nullptr, 1);
  CHECK(!empty.invariants_hold());
  WindowTable zero(platform);
  av::glicd::WindowTableProbe::plant(zero, &window, 0);
  CHECK(!zero.invariants_hold());
  int other = 0;
  WindowTable both(platform);
  av::glicd::WindowTableProbe::plant(both, &window, 0);
  av::glicd::WindowTableProbe::plant(both, &other, 0);
  CHECK(!both.invariants_hold());
  WindowTable table(platform);
  av::glicd::WindowTableProbe::plant(table, &window, 3);
  CHECK(table.invariants_hold());
  av::glicd::WindowTableProbe::corrupt_table(table);
  CHECK(!table.invariants_hold()); }
