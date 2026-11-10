#include "icd/opencl/cl_build_options.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"
#include "icd/opencl/cl_object.hpp"

namespace av::clicd {

namespace {

size_t length_of(const char *text)
{
    AV_SCOPE("length_of");
    size_t length = 0u;
    while (text[length] != '\0') ++length;
    return length;
}

bool at_token(const char *text, const char *token, size_t token_length)
{
    AV_SCOPE("at_token");
    for (size_t index = 0u; index < token_length; ++index)
        if (text[index] != token[index]) return false;
    return text[token_length] == '\0' || text[token_length] == ' ';
}

}

ClBuildOptions::ClBuildOptions(BlockSource &memory, const char *asked)
    : memory_(memory), asked_(asked != nullptr ? asked : ""), host_(nullptr)
{
    AV_SCOPE("ClBuildOptions::ClBuildOptions");
    const size_t asked_length = length_of(asked_);
    const size_t option_length = length_of(arguments_option);
    if (has_token(arguments_option)) return;
    host_ = static_cast<char *>(memory_.allocate(asked_length + 1u + option_length + 1u, Pool::paged));
    if (host_ == nullptr) return;
    size_t at = 0u;
    for (size_t index = 0u; index < asked_length; ++index) host_[at++] = asked_[index];
    if (asked_length != 0u) host_[at++] = ' ';
    for (size_t index = 0u; index < option_length; ++index) host_[at++] = arguments_option[index];
    host_[at] = '\0';
}

ClBuildOptions::~ClBuildOptions()
{
    AV_SCOPE("ClBuildOptions::~ClBuildOptions");
    if (host_ != nullptr) memory_.release(host_);
}

bool ClBuildOptions::usable() const
{
    AV_SCOPE("ClBuildOptions::usable");
    return host_ != nullptr || has_token(arguments_option);
}

const char *ClBuildOptions::for_host() const
{
    AV_SCOPE("ClBuildOptions::for_host");
    return host_ != nullptr ? host_ : asked_;
}

bool ClBuildOptions::has_token(const char *token) const
{
    AV_SCOPE("ClBuildOptions::has_token");
    const size_t token_length = length_of(token);
    for (const char *at = asked_; *at != '\0'; ++at) {
        if ((at == asked_ || at[-1] == ' ') && at_token(at, token, token_length)) return true;
    }
    return false;
}

bool ClBuildOptions::standard_allows_non_uniform() const
{
    AV_SCOPE("ClBuildOptions::standard_allows_non_uniform");
    static constexpr char standard[] = "-cl-std=CL";
    const size_t          prefix = length_of(standard);
    bool                  allowed = false;
    for (const char *at = asked_; *at != '\0'; ++at) {
        if (at != asked_ && at[-1] != ' ') continue;
        size_t index = 0u;
        while (index < prefix && at[index] == standard[index]) ++index;
        if (index != prefix) continue;
        allowed = at[prefix] >= '2' && at[prefix] <= '9' && at[prefix + 1u] == '.';
    }
    return allowed;
}

uint32_t ClBuildOptions::traits() const
{
    AV_SCOPE("ClBuildOptions::traits");
    uint32_t traits = ClObject::program_typed_arguments;
    if (has_token(arguments_option)) traits |= ClObject::program_arguments_asked;
    if (standard_allows_non_uniform()) traits |= ClObject::program_non_uniform;
    return traits;
}

size_t ClBuildOptions::without_arguments_option(char *text, size_t length)
{
    AV_SCOPE("ClBuildOptions::without_arguments_option");
    const size_t option_length = length_of(arguments_option);
    if (length < option_length) return length;
    const size_t start = length - option_length;
    for (size_t index = 0u; index < option_length; ++index)
        if (text[start + index] != arguments_option[index]) return length;
    const size_t kept = start != 0u && text[start - 1u] == ' ' ? start - 1u : start;
    text[kept] = '\0';
    return kept;
}

}
