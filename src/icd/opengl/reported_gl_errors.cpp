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
    return GlErrorWord(word).published();
}

uint32_t ReportedGlErrors::take(uint64_t word)
{
    AV_SCOPE("ReportedGlErrors::take");
    const GlErrorWord published(word);
    for (uint32_t kind = 0u; kind < GlErrorWord::kinds; kind++) {
        const uint8_t count = published.count(kind);
        if (count == reported_[kind]) continue;
        reported_[kind] = count;
        return GlErrorWord::first_error + kind;
    }
    return 0u;
}

}
