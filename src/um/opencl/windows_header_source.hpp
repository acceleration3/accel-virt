#ifndef AV_WINDOWS_HEADER_SOURCE_HPP
#define AV_WINDOWS_HEADER_SOURCE_HPP

#include "icd/opencl/header_source.hpp"

#include <stdint.h>

namespace av {

class WindowsHeaderSource final : public clicd::HeaderSource {
public:
    WindowsHeaderSource();

    bool fetch(BlockSource &memory, const char *directory, const char *name, char *&text, size_t &bytes) override;

    uint32_t reads() const;

private:
    uint32_t reads_;
};

}

#endif
