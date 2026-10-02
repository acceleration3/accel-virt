#include "executor/opengl/gl_error_tally.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

GlErrorTally::GlErrorTally() : counts_{}
{
    AV_SCOPE("GlErrorTally::GlErrorTally");
}

void GlErrorTally::record(uint32_t error)
{
    AV_SCOPE("GlErrorTally::record");
    if (error < first_error || error - first_error >= kinds) return;
    counts_[error - first_error]++;
}

uint64_t GlErrorTally::word() const
{
    AV_SCOPE("GlErrorTally::word");
    uint64_t word = published;
    for (uint32_t kind = 0u; kind < kinds; kind++) word |= uint64_t{counts_[kind]} << (8u * kind);
    return word;
}

}  // namespace av
