#include "um/common/driver_seams.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DriverSeams::DriverSeams(const wchar_t *stem, const char *prefix)
    : heap_(), file_(stem), log_(heap_, file_, prefix), kmt_(), trace_()
{
    AV_SCOPE("DriverSeams::DriverSeams");
    file_.open();
}

Status DriverSeams::initialize()
{
    AV_SCOPE("DriverSeams::initialize");
    return kmt_.initialize();
}

ProcessHeap &DriverSeams::heap()
{
    AV_SCOPE("DriverSeams::heap");
    return heap_;
}

KeyedLog &DriverSeams::log()
{
    AV_SCOPE("DriverSeams::log");
    return log_;
}

GdiKmt &DriverSeams::kmt()
{
    AV_SCOPE("DriverSeams::kmt");
    return kmt_;
}

ProcessTrace &DriverSeams::trace()
{
    AV_SCOPE("DriverSeams::trace");
    return trace_;
}

}
