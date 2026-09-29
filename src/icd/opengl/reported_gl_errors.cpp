#include "icd/opengl/reported_gl_errors.hpp"

#include "core/trace/function_scope.hpp"

namespace av::glicd {

ReportedGlErrors::ReportedGlErrors() : reported_{}
{
    AV_SCOPE("ReportedGlErrors::ReportedGlErrors");
}

bool ReportedGlErrors::readable(uint64_t word) const
{
    AV_SCOPE("ReportedGlErrors::readable");
    return (word & published) != 0u;
}

uint32_t ReportedGlErrors::take(uint64_t word)
{
    AV_SCOPE("ReportedGlErrors::take");
    for (uint32_t kind = 0u; kind < kinds; kind++) {
        const auto count = static_cast<uint8_t>(word >> (8u * kind));
        if (count == reported_[kind]) continue;
        reported_[kind] = count;
        return first_error + kind;
    }
    return 0u;
}

}  // namespace av::glicd
