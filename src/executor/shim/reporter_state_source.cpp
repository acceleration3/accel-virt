#include "executor/shim/reporter_state_source.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"

namespace av {

ReporterStateSource::ReporterStateSource(StateReporter &reporter) : reporter_(reporter)
{
    AV_SCOPE("ReporterStateSource::ReporterStateSource");
}

size_t ReporterStateSource::dump(char *buffer, size_t capacity, const char *&error)
{
    AV_SCOPE("ReporterStateSource::dump");
    error = nullptr;
    StateWriter writer(buffer, capacity);
    reporter_.report(writer);
    return writer.length();
}

}
