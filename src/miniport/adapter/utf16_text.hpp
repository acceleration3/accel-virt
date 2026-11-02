#ifndef AV_UTF16_TEXT_HPP
#define AV_UTF16_TEXT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class Utf16Text {
public:
    static constexpr size_t capacity = 64u;

    explicit Utf16Text(const char *utf8);

    const uint16_t *units() const;
    uint32_t        bytes() const;
    bool            empty() const;

private:
    static constexpr uint16_t replacement = 0xFFFDu;

    uint32_t decode(const unsigned char *utf8, size_t &taken) const;
    bool     append(uint32_t code_point);

    uint16_t units_[capacity];
    size_t   length_;
};

}

#endif
