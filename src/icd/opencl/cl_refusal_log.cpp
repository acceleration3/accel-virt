#include "icd/opencl/cl_refusal_log.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClRefusalLog::ClRefusalLog(KeyedLog &lines) : lines_(lines)
{
    AV_SCOPE("ClRefusalLog::ClRefusalLog");
}

void ClRefusalLog::note_query(const char *entry_point, uint32_t query, int32_t error)
{
    AV_SCOPE("ClRefusalLog::note_query");
    if (entry_point == nullptr || error >= 0 || !lines_.first(entry_point, static_cast<int32_t>(query))) return;
    char       bytes[48];
    TextBuffer answer(bytes, sizeof bytes);
    answer.append("0x");
    answer.append_hex(query, 4u);
    answer.append(" answered -");
    answer.append_decimal(static_cast<uint64_t>(-static_cast<int64_t>(error)));
    lines_.name(entry_point, answer.text());
}

void ClRefusalLog::note_map(const char *why, uint64_t bytes, uint32_t held)
{
    AV_SCOPE("ClRefusalLog::note_map");
    if (why == nullptr || !lines_.first(why, 0)) return;
    char       text[96];
    TextBuffer line(text, sizeof text);
    line.append("of ");
    line.append_decimal(bytes);
    line.append(" bytes, with ");
    line.append_decimal(held);
    line.append(" mappings held");
    lines_.name(why, line.text());
}

void ClRefusalLog::note_released_mapped(uint32_t mappings)
{
    AV_SCOPE("ClRefusalLog::note_released_mapped");
    static const char key[] = "clReleaseMemObject: a buffer released while still mapped";
    if (mappings == 0u || !lines_.first(key, 0)) return;
    char       text[64];
    TextBuffer line(text, sizeof text);
    line.append_decimal(mappings);
    line.append(" mappings went with it");
    lines_.name(key, line.text());
}

void ClRefusalLog::note_ran_ahead_of_user_event()
{
    AV_SCOPE("ClRefusalLog::note_ran_ahead_of_user_event");
    static const char key[] = "a non-blocking transfer waits on a user event not yet set";
    if (!lines_.first(key, 0)) return;
    lines_.name(key, "it runs now rather than wait for a set the same thread may make only after it returns");
}

uint32_t ClRefusalLog::named() const
{
    AV_SCOPE("ClRefusalLog::named");
    return lines_.keys();
}

}  // namespace av::clicd
