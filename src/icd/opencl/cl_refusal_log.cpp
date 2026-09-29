#include "icd/opencl/cl_refusal_log.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/common/log_sink.hpp"
#include "icd/common/thread_lock.hpp"

namespace av::clicd {

ClRefusalLog::ClRefusalLog(icd::LogSink &log, icd::ThreadLock &lock)
    : log_(log), lock_(lock), named_(), errors_(), count_(0u)
{
    AV_SCOPE("ClRefusalLog::ClRefusalLog");
}

void ClRefusalLog::note(const char *entry_point)
{
    AV_SCOPE("ClRefusalLog::note");
    if (entry_point == nullptr || !first(entry_point, 0)) return;
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkcl: an entry point this ICD does not serve was called: ");
    line.append(entry_point);
    log_.write(line.text());
}

void ClRefusalLog::note_failure(const char *entry_point, int32_t error)
{
    AV_SCOPE("ClRefusalLog::note_failure");
    if (entry_point == nullptr || error >= 0 || !first(entry_point, error)) return;
    char       bytes[128];
    TextBuffer line(bytes, sizeof bytes);
    line.append("rvkcl: ");
    line.append(entry_point);
    line.append(" answered -");
    line.append_decimal(static_cast<uint64_t>(-static_cast<int64_t>(error)));
    log_.write(line.text());
}

bool ClRefusalLog::first(const char *entry_point, int32_t error)
{
    AV_SCOPE("ClRefusalLog::first");
    lock_.acquire();
    bool seen = false;
    for (uint32_t index = 0u; index < count_ && !seen; ++index)
        seen = named_[index] == entry_point && errors_[index] == error;
    const bool fresh = !seen && count_ < max_named;
    if (fresh) {
        named_[count_] = entry_point;
        errors_[count_] = error;
        count_++;
    }
    lock_.release();
    return fresh;
}

uint32_t ClRefusalLog::named() const
{
    AV_SCOPE("ClRefusalLog::named");
    return count_;
}

}  // namespace av::clicd
