#include "icd/opencl/cl_option_judge.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

namespace {

bool starts(const char *text, unsigned length, const char *prefix)
{
    AV_SCOPE("starts");
    unsigned index = 0u;
    while (prefix[index] != '\0') {
        if (index >= length || text[index] != prefix[index]) return false;
        ++index;
    }
    return true;
}

bool same(const char *text, unsigned length, const char *word)
{
    AV_SCOPE("same");
    unsigned size = 0u;
    while (word[size] != '\0') ++size;
    return size == length && starts(text, length, word);
}

}

ClOptionJudge::ClOptionJudge(const char *options) : options_(options != nullptr ? options : "")
{
    AV_SCOPE("ClOptionJudge::ClOptionJudge");
}

bool ClOptionJudge::token_valid(const char *start, unsigned length, bool &takes_next) const
{
    AV_SCOPE("ClOptionJudge::token_valid");
    takes_next = false;
    if (same(start, length, "-D") || same(start, length, "-I") || same(start, length, "-x")) {
        takes_next = true;
        return true;
    }
    if (starts(start, length, "-D") || starts(start, length, "-I") || starts(start, length, "-O") ||
        starts(start, length, "-W") || starts(start, length, "-f") || starts(start, length, "-m") ||
        starts(start, length, "-g") || starts(start, length, "-spir-std="))
        return true;
    if (same(start, length, "-w") || same(start, length, "-create-library") ||
        same(start, length, "-enable-link-options"))
        return true;
    if (starts(start, length, "-cl-std=")) {
        const char *const value = start + 8u;
        const unsigned    value_length = length - 8u;
        return same(value, value_length, "CL1.0") || same(value, value_length, "CL1.1") ||
               same(value, value_length, "CL1.2") || same(value, value_length, "CL2.0") ||
               same(value, value_length, "CL3.0") || starts(value, value_length, "CLC++");
    }
    return starts(start, length, "-cl-");
}

bool ClOptionJudge::valid() const
{
    AV_SCOPE("ClOptionJudge::valid");
    unsigned at = 0u;
    bool     skip = false;
    while (options_[at] != '\0') {
        while (options_[at] == ' ') ++at;
        if (options_[at] == '\0') break;
        const unsigned start = at;
        bool           quoted = false;
        while (options_[at] != '\0' && (quoted || options_[at] != ' ')) {
            if (options_[at] == '"') quoted = !quoted;
            ++at;
        }
        if (skip) {
            skip = false;
            continue;
        }
        bool takes_next = false;
        if (!token_valid(options_ + start, at - start, takes_next)) return false;
        skip = takes_next;
    }
    return true;
}

}
