/* A window's pixel format: set once, as Windows allows, on a real window, up
 * to 256 windows. */
#include "core/trace/state_writer.hpp"
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/window_table.hpp"

#include <cstring>

using av::Status;
using av::glicd::WindowTable;

namespace av::glicd {

struct WindowTableProbe {
    static void overfill(WindowTable &table) { table.count_ = WindowTable::max_windows + 1u; }
    static void empty_row(WindowTable &table) { table.windows_[0] = nullptr; table.count_ = 1u; }
    static void zero_format(WindowTable &table, void *window) { table.windows_[0] = window; table.formats_[0] = 0; table.count_ = 1u; }
    static void repeat(WindowTable &table, void *window)
    {
        table.windows_[0] = window;
        table.windows_[1] = window;
        table.formats_[0] = 1;
        table.formats_[1] = 1;
        table.count_ = 2u;
    }
};

}  // namespace av::glicd

TEST(window_table, a_window_takes_one_format_and_keeps_it)
{ WindowTable table;
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

TEST(window_table, refuses_no_window_a_format_below_one_and_a_257th_window)
{ WindowTable table;
  int window = 0;
  CHECK_STATUS(table.set_format(nullptr, 1), Status::invalid_parameter);
  CHECK_STATUS(table.set_format(&window, 0), Status::invalid_parameter);
  CHECK_STATUS(table.set_format(&window, -1), Status::invalid_parameter);
  CHECK_EQ(table.held(), 0u);
  int windows[WindowTable::max_windows + 1u] = {};
  for (int &each : windows) {
      const Status set = table.set_format(&each, 2);
      CHECK_STATUS(set, &each == &windows[WindowTable::max_windows] ? Status::no_space : Status::ok);
  }
  CHECK_EQ(table.held(), WindowTable::max_windows);
  CHECK_EQ(table.format_of(&windows[WindowTable::max_windows]), 0);
  CHECK(table.invariants_hold()); }

TEST(window_table, reports_each_window_and_its_format)
{ WindowTable table;
  CHECK_STATUS(table.set_format(reinterpret_cast<void *>(0x1234u), 5), Status::ok);
  char bytes[256];
  av::StateWriter writer(bytes, sizeof bytes);
  table.report(writer);
  CHECK(std::strcmp(writer.dump(),
                    "windows/0 held=1\nwindows/0/window/0 hwnd=0x0000000000001234 format=5") == 0); }

TEST(window_table_invariants, fail_for_rows_no_set_writes)
{ int window = 0;
  WindowTable over;
  av::glicd::WindowTableProbe::overfill(over);
  CHECK(!over.invariants_hold());
  WindowTable empty;
  av::glicd::WindowTableProbe::empty_row(empty);
  CHECK(!empty.invariants_hold());
  WindowTable zero;
  av::glicd::WindowTableProbe::zero_format(zero, &window);
  CHECK(!zero.invariants_hold());
  WindowTable repeated;
  av::glicd::WindowTableProbe::repeat(repeated, &window);
  CHECK(!repeated.invariants_hold()); }
