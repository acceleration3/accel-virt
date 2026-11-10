#include "icd/opencl/cl_pragma_judge.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

bool blank(char c)
{
    AV_SCOPE("blank");
    return c == ' ' || c == '\t';
}

size_t skip(const char *text, size_t at, size_t bytes)
{
    AV_SCOPE("skip");
    while (at < bytes && blank(text[at])) ++at;
    return at;
}

bool word(const char *text, size_t &at, size_t bytes, const char *wanted)
{
    AV_SCOPE("word");
    size_t index = skip(text, at, bytes);
    size_t used = 0u;
    while (wanted[used] != '\0') {
        if (index + used >= bytes || text[index + used] != wanted[used]) return false;
        ++used;
    }
    at = index + used;
    return true;
}

}

ClPragmaJudge::ClPragmaJudge(const char *source, size_t bytes) : source_(source), bytes_(source != nullptr ? bytes : 0u)
{
    AV_SCOPE("ClPragmaJudge::ClPragmaJudge");
}

bool ClPragmaJudge::enables(const char *extension) const
{
    AV_SCOPE("ClPragmaJudge::enables");
    for (size_t line = 0u; line < bytes_;) {
        size_t at = line;
        if (at < bytes_ && source_[skip(source_, at, bytes_)] == '#') {
            at = skip(source_, at, bytes_) + 1u;
            size_t cursor = at;
            if (word(source_, cursor, bytes_, "pragma") && word(source_, cursor, bytes_, "OPENCL") &&
                word(source_, cursor, bytes_, "EXTENSION") && word(source_, cursor, bytes_, extension)) {
                size_t after = skip(source_, cursor, bytes_);
                if (after < bytes_ && source_[after] == ':') {
                    ++after;
                    if (word(source_, after, bytes_, "enable")) return true;
                }
            }
        }
        while (line < bytes_ && source_[line] != '\n') ++line;
        ++line;
    }
    return false;
}

}
