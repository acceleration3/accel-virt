#include "icd/opencl/cl_refusal_log.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClRefusalLog::ClRefusalLog(LogSink &log) : lines_(log, "avcl: ")
{
    AV_SCOPE("ClRefusalLog::ClRefusalLog");
}

void ClRefusalLog::note(const char *entry_point)
{
    AV_SCOPE("ClRefusalLog::note");
    if (entry_point == nullptr || !lines_.first(entry_point, 0)) return;
    lines_.name("an entry point this ICD does not serve was called:", entry_point);
}

void ClRefusalLog::note_failure(const char *entry_point, int32_t error)
{
    AV_SCOPE("ClRefusalLog::note_failure");
    if (entry_point == nullptr || error >= 0 || !lines_.first(entry_point, error)) return;
    char       bytes[32];
    TextBuffer answer(bytes, sizeof bytes);
    answer.append("answered -");
    answer.append_decimal(static_cast<uint64_t>(-static_cast<int64_t>(error)));
    lines_.name(entry_point, answer.text());
}

uint32_t ClRefusalLog::named() const
{
    AV_SCOPE("ClRefusalLog::named");
    return lines_.keys();
}

}  // namespace av::clicd
