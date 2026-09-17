#include "icd/opengl/window_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"

namespace av::glicd {

WindowTable::WindowTable() : windows_{}, formats_{}, count_(0u)
{
    AV_SCOPE("WindowTable::WindowTable");
}

Status WindowTable::set_format(void *window, int32_t format)
{
    AV_SCOPE("WindowTable::set_format");
    if (window == nullptr || format < 1) return Status::invalid_parameter;
    if (format_of(window) != 0) return Status::conflict;
    if (count_ == max_windows) return Status::no_space;
    windows_[count_] = window;
    formats_[count_] = format;
    count_++;
    return Status::ok;
}

int32_t WindowTable::format_of(void *window) const
{
    AV_SCOPE("WindowTable::format_of");
    for (uint32_t row = 0u; row < count_; row++)
        if (windows_[row] == window) return formats_[row];
    return 0;
}

uint32_t WindowTable::held() const
{
    AV_SCOPE("WindowTable::held");
    return count_;
}

void WindowTable::report(StateWriter &writer) const
{
    AV_SCOPE("WindowTable::report");
    writer.open("windows", 0u);
    writer.number("held", count_);
    for (uint32_t row = 0u; row < count_; row++) {
        writer.open("window", row);
        writer.hex("hwnd", static_cast<uint64_t>(reinterpret_cast<uintptr_t>(windows_[row])));
        writer.number("format", static_cast<uint32_t>(formats_[row]));
        writer.close();
    }
    writer.close();
}

bool WindowTable::invariants_hold() const
{
    AV_SCOPE("WindowTable::invariants_hold");
    if (count_ > max_windows) return false;
    for (uint32_t row = 0u; row < count_; row++) {
        if (windows_[row] == nullptr || formats_[row] < 1) return false;
        for (uint32_t earlier = 0u; earlier < row; earlier++)
            if (windows_[earlier] == windows_[row]) return false;
    }
    return true;
}

}  // namespace av::glicd
