#ifndef AV_CLICD_CL_BUILD_OPTIONS_HPP
#define AV_CLICD_CL_BUILD_OPTIONS_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

struct ClBuildOptionsProbe;

class ClBuildOptions {
public:
    static constexpr char arguments_option[] = "-cl-kernel-arg-info";

    ClBuildOptions(BlockSource &memory, const char *asked);
    ~ClBuildOptions();
    ClBuildOptions(const ClBuildOptions &) = delete;
    ClBuildOptions &operator=(const ClBuildOptions &) = delete;

    bool        usable() const;
    const char *for_host() const;
    uint32_t    traits() const;

    size_t without_arguments_option(char *text, size_t length) const;

private:
    friend struct ClBuildOptionsProbe;

    bool has_token(const char *token) const;
    bool standard_allows_non_uniform() const;

    BlockSource &memory_;
    const char  *asked_;
    char        *host_;
};

}

#endif
