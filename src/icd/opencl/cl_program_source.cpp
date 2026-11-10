#include "icd/opencl/cl_program_source.hpp"

#include "core/platform/block_source.hpp"
#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClProgramSource::ClProgramSource(BlockSource &memory) : memory_(memory), text_(nullptr), text_bytes_(0u), options_(nullptr), host_options_(nullptr)
{
    AV_SCOPE("ClProgramSource::ClProgramSource");
}

ClProgramSource::~ClProgramSource()
{
    AV_SCOPE("ClProgramSource::~ClProgramSource");
    if (text_ != nullptr) memory_.release(text_);
    if (options_ != nullptr) memory_.release(options_);
    if (host_options_ != nullptr) memory_.release(host_options_);
}

bool ClProgramSource::keep_text(const char *const *strings, const size_t *lengths, uint32_t count)
{
    AV_SCOPE("ClProgramSource::keep_text");
    size_t total = 0u;
    for (uint32_t index = 0u; index < count; ++index) {
        if (strings[index] == nullptr) continue;
        size_t length = lengths != nullptr ? lengths[index] : 0u;
        if (length == 0u) while (strings[index][length] != '\0') ++length;
        total += length;
    }
    char *const block = static_cast<char *>(memory_.allocate(total + 1u, Pool::paged));
    if (block == nullptr) return false;
    size_t at = 0u;
    for (uint32_t index = 0u; index < count; ++index) {
        if (strings[index] == nullptr) continue;
        size_t length = lengths != nullptr ? lengths[index] : 0u;
        if (length == 0u) while (strings[index][length] != '\0') ++length;
        for (size_t letter = 0u; letter < length; ++letter) block[at++] = strings[index][letter];
    }
    block[at] = '\0';
    if (text_ != nullptr) memory_.release(text_);
    text_ = block;
    text_bytes_ = at;
    return true;
}

bool ClProgramSource::keep_options(const char *options)
{
    AV_SCOPE("ClProgramSource::keep_options");
    size_t length = 0u;
    if (options != nullptr) while (options[length] != '\0') ++length;
    char *const block = static_cast<char *>(memory_.allocate(length + 1u, Pool::paged));
    if (block == nullptr) return false;
    for (size_t letter = 0u; letter < length; ++letter) block[letter] = options[letter];
    block[length] = '\0';
    if (options_ != nullptr) memory_.release(options_);
    options_ = block;
    return true;
}

bool ClProgramSource::keep_host_options(const char *options)
{
    AV_SCOPE("ClProgramSource::keep_host_options");
    size_t length = 0u;
    if (options != nullptr) while (options[length] != '\0') ++length;
    char *const block = static_cast<char *>(memory_.allocate(length + 1u, Pool::paged));
    if (block == nullptr) return false;
    for (size_t letter = 0u; letter < length; ++letter) block[letter] = options[letter];
    block[length] = '\0';
    if (host_options_ != nullptr) memory_.release(host_options_);
    host_options_ = block;
    return true;
}

const char *ClProgramSource::host_options() const
{
    AV_SCOPE("ClProgramSource::host_options");
    return host_options_;
}

const char *ClProgramSource::text() const
{
    AV_SCOPE("ClProgramSource::text");
    return text_;
}

size_t ClProgramSource::text_bytes() const
{
    AV_SCOPE("ClProgramSource::text_bytes");
    return text_bytes_;
}

const char *ClProgramSource::options() const
{
    AV_SCOPE("ClProgramSource::options");
    return options_;
}

}
