#include "icd/common/report_gate.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_writer.hpp"
#include "icd/common/thread_lock.hpp"

namespace av::icd {

ReportGate::ReportGate(ThreadLock &lock, StateWriter &writer, const char *section) : lock_(lock), held_(false)
{
    AV_SCOPE("ReportGate::ReportGate");
    writer.open(section, 0u);
    held_ = lock_.try_acquire();
    writer.flag("busy", !held_);
    if (!held_) writer.close();
}

ReportGate::~ReportGate()
{
    AV_SCOPE("ReportGate::~ReportGate");
    if (held_) lock_.release();
}

bool ReportGate::held() const
{
    AV_SCOPE("ReportGate::held");
    return held_;
}

}  // namespace av::icd
