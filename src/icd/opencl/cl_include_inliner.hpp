#ifndef AV_CLICD_CL_INCLUDE_INLINER_HPP
#define AV_CLICD_CL_INCLUDE_INLINER_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

class HeaderSource;
struct ClIncludeInlinerProbe;

class ClIncludeInliner {
public:
    static constexpr uint32_t most_directories = 16u;
    static constexpr uint32_t most_depth = 16u;
    static constexpr uint32_t most_once = 64u;

    ClIncludeInliner(BlockSource &memory, HeaderSource &headers);
    ~ClIncludeInliner();
    ClIncludeInliner(const ClIncludeInliner &) = delete;
    ClIncludeInliner &operator=(const ClIncludeInliner &) = delete;

    [[nodiscard]] Status read_options(const char *options);
    uint32_t             directories() const;
    const char          *host_options() const;

    [[nodiscard]] Status expand(const char *source, size_t bytes);
    const char          *text() const;
    size_t               text_bytes() const;

private:
    friend struct ClIncludeInlinerProbe;

    [[nodiscard]] Status expand_into(const char *source, size_t bytes, uint32_t depth);
    [[nodiscard]] Status add_line(const char *line, size_t bytes);
    bool                 include_name(const char *line, size_t bytes, bool &quoted, char *name, size_t room) const;
    bool                 once_seen(const char *directory, const char *name, const char *text, size_t bytes);

    BlockSource &memory_;
    HeaderSource &headers_;
    char        *host_;
    char        *directory_text_;
    uint32_t     directory_at_[most_directories];
    uint32_t     directories_;
    GrowableArray<char> out_;
    char        *once_[most_once];
    uint32_t     once_count_;
};

}

#endif
