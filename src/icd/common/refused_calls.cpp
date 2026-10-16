#include "icd/common/refused_calls.hpp"

#include "core/log/keyed_log.hpp"
#include "core/log/log_line.hpp"
#include "core/trace/function_scope.hpp"

namespace av::icd {

RefusedCalls::RefusedCalls(KeyedLog &lines) : lines_(lines)
{
    AV_SCOPE("RefusedCalls::RefusedCalls");
}

void RefusedCalls::unserved(const char *entry)
{
    AV_SCOPE("RefusedCalls::unserved");
    if (entry == nullptr || !lines_.first(entry, 0)) return;
    lines_.name("an entry point this driver does not serve was called:", entry);
}

void RefusedCalls::failed(const char *entry, int64_t code)
{
    AV_SCOPE("RefusedCalls::failed");
    if (entry == nullptr || !lines_.first(entry, code)) return;
    LogLine line(lines_);
    line.text(entry).text(" answered ");
    if (code < hresult_floor)
        line.hex(static_cast<uint32_t>(code), 8u);
    else if (code < 0)
        line.text("-").number(static_cast<uint64_t>(-code));
    else
        line.number(static_cast<uint64_t>(code));
    line.write();
}

void RefusedCalls::dropped(const char *api, const char *entry, size_t written)
{
    AV_SCOPE("RefusedCalls::dropped");
    if (entry == nullptr || !lines_.first(entry, dropped_code)) return;
    LogLine(lines_)
        .text(api)
        .text(" ")
        .text(entry)
        .text(" not sent: its encoding overflowed at byte ")
        .number(written)
        .write();
}

}  // namespace av::icd
