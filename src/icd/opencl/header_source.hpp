#ifndef AV_CLICD_HEADER_SOURCE_HPP
#define AV_CLICD_HEADER_SOURCE_HPP

#include <stddef.h>

namespace av {
class BlockSource;
}

namespace av::clicd {

class HeaderSource {
public:
    HeaderSource(const HeaderSource &) = delete;
    HeaderSource &operator=(const HeaderSource &) = delete;

    virtual bool fetch(BlockSource &memory, const char *directory, const char *name, char *&text, size_t &bytes) = 0;

protected:
    HeaderSource() = default;
    ~HeaderSource() = default;
};

}

#endif
