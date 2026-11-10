#ifndef AV_CLICD_CL_PROGRAM_SOURCE_HPP
#define AV_CLICD_CL_PROGRAM_SOURCE_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

class ClProgramSource {
public:
    explicit ClProgramSource(BlockSource &memory);
    ~ClProgramSource();
    ClProgramSource(const ClProgramSource &) = delete;
    ClProgramSource &operator=(const ClProgramSource &) = delete;

    [[nodiscard]] bool keep_text(const char *const *strings, const size_t *lengths, uint32_t count);
    [[nodiscard]] bool keep_options(const char *options);
    [[nodiscard]] bool keep_host_options(const char *options);

    const char *text() const;
    size_t      text_bytes() const;
    const char *options() const;
    const char *host_options() const;

private:
    BlockSource &memory_;
    char        *text_;
    size_t       text_bytes_;
    char        *options_;
    char        *host_options_;
};

}

#endif
