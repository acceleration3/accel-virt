#include "icd/opengl/window_table.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/window_extent.hpp"
#include "icd/common/window_surface.hpp"

namespace av::glicd {

WindowTable::WindowTable(BlockSource &memory) : formats_(memory, Pool::paged)
{
    AV_SCOPE("WindowTable::WindowTable");
}

uint64_t WindowTable::key_of(void *window) const
{
    AV_SCOPE("WindowTable::key_of");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(window));
}

Status WindowTable::set_format(void *window, int32_t format)
{
    AV_SCOPE("WindowTable::set_format");
    if (window == nullptr || format < 1) return Status::invalid_parameter;
    uint32_t slot = 0u;
    return formats_.insert(key_of(window), format, slot);
}

int32_t WindowTable::format_of(void *window) const
{
    AV_SCOPE("WindowTable::format_of");
    const int32_t *const format = formats_.find(key_of(window));
    return format != nullptr ? *format : 0;
}

uint32_t WindowTable::held() const
{
    AV_SCOPE("WindowTable::held");
    return formats_.count();
}

void WindowTable::drop_gone(icd::WindowSurface &surface)
{
    AV_SCOPE("WindowTable::drop_gone");
    formats_.erase_if([&surface](uint64_t window, int32_t &) {
        return icd::WindowExtent(surface, reinterpret_cast<void *>(static_cast<uintptr_t>(window))).look() ==
               icd::WindowExtent::Look::gone;
    });
}

void WindowTable::report(StateWriter &writer) const
{
    AV_SCOPE("WindowTable::report");
    writer.open("windows", 0u);
    writer.number("held", formats_.count());
    uint32_t row = 0u;
    formats_.for_each([&writer, &row](uint64_t window, const int32_t &format) {
        writer.open("window", row++);
        writer.hex("hwnd", window);
        writer.number("format", static_cast<uint32_t>(format));
        writer.close();
    });
    writer.close();
}

bool WindowTable::invariants_hold() const
{
    AV_SCOPE("WindowTable::invariants_hold");
    bool formatted = true;
    formats_.for_each([&formatted](uint64_t window, const int32_t &format) {
        formatted = formatted && window != 0u && format >= 1;
    });
    return formatted && formats_.invariants_hold();
}

}
