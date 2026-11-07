#ifndef AV_MEM_INFO_TEXT_HPP
#define AV_MEM_INFO_TEXT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class MemInfoText {
public:
    MemInfoText(const char *text, size_t length);

    uint64_t total_bytes() const;
    uint64_t available_bytes() const;

private:
    uint64_t field(const char *key, size_t key_length) const;

    const char *text_;
    size_t      length_;
};

}

#endif
