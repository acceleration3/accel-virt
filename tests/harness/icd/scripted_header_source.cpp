#include "harness/icd/scripted_header_source.hpp"

#include "core/platform/block_source.hpp"

#include <cstring>

namespace av {

ScriptedHeaderSource::ScriptedHeaderSource() : files_(), count_(0u), asked_(0u) {}

void ScriptedHeaderSource::add(const char *directory, const char *name, const char *text)
{
    if (count_ < most_files) files_[count_++] = File{directory, name, text};
}

bool ScriptedHeaderSource::fetch(BlockSource &memory, const char *directory, const char *name, char *&text,
                                 std::size_t &bytes)
{
    ++asked_;
    text = nullptr;
    bytes = 0u;
    for (std::uint32_t index = 0u; index < count_; ++index) {
        if (std::strcmp(files_[index].directory_, directory) != 0 || std::strcmp(files_[index].name_, name) != 0) continue;
        const std::size_t length = std::strlen(files_[index].text_);
        char *const       block = static_cast<char *>(memory.allocate(length + 1u, Pool::paged));
        if (block == nullptr) return false;
        std::memcpy(block, files_[index].text_, length + 1u);
        text = block;
        bytes = length;
        return true;
    }
    return false;
}

std::uint32_t ScriptedHeaderSource::asked() const { return asked_; }

}
