#ifndef AV_SCRIPTED_HEADER_SOURCE_HPP
#define AV_SCRIPTED_HEADER_SOURCE_HPP

#include "icd/opencl/header_source.hpp"

#include <cstdint>

namespace av {

class ScriptedHeaderSource final : public clicd::HeaderSource {
public:
    static constexpr std::uint32_t most_files = 8u;

    ScriptedHeaderSource();

    void          add(const char *directory, const char *name, const char *text);
    bool          fetch(BlockSource &memory, const char *directory, const char *name, char *&text,
                        std::size_t &bytes) override;
    std::uint32_t asked() const;

private:
    struct File {
        const char *directory_;
        const char *name_;
        const char *text_;
    };

    File          files_[most_files];
    std::uint32_t count_;
    std::uint32_t asked_;
};

}

#endif
