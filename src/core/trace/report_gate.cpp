#include "core/trace/report_gate.hpp"

#include "core/platform/thread_lock.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

ReportGate::ReportGate(ThreadLock &lock, StateWriter &writer, const char *section) : lock_(lock), held_(false)
{
    writer.open(section, 0u);
    held_ = lock_.try_acquire();
    writer.flag("busy", !held_);
    if (!held_) writer.close();
}

ReportGate::~ReportGate()
{
    if (held_) lock_.release();
}

bool ReportGate::held() const
{
    return held_;
}

}
