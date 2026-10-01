#include "device/host/stdio_log.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

StdioLog::StdioLog(FILE *stream) : stream_(stream)
{
    AV_SCOPE("StdioLog::StdioLog");
}

/* One fprintf, so a line from one thread is never split by another's. */
void StdioLog::write(const char *line)
{
    AV_SCOPE("StdioLog::write");
    fprintf(stream_, "%s\n", line);
}

}  // namespace av
