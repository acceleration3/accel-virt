#include "um/common/driver_seams.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

DriverSeams::DriverSeams(const wchar_t *stem) : heap_(), log_(stem), kmt_(), trace_()
{
    AV_SCOPE("DriverSeams::DriverSeams");
    log_.open();
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

FileLog &DriverSeams::log()
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

}  // namespace av
