#ifndef AV_MEM_INFO_TEXT_HPP
#define AV_MEM_INFO_TEXT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class MemInfoText {
public:
    MemInfoText(const char *text, size_t length);

    uint64_t total_bytes() const;

private:
    const char *text_;
    size_t      length_;
};

}

#endif
