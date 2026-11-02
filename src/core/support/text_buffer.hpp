#ifndef AV_TEXT_BUFFER_HPP
#define AV_TEXT_BUFFER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class TextBuffer {
public:
    TextBuffer(char *destination, size_t capacity);
    TextBuffer(const TextBuffer &) = delete;
    TextBuffer &operator=(const TextBuffer &) = delete;

    void        append(const char *text);
    void        append_decimal(uint64_t value);
    void        append_hex(uint64_t value, uint32_t digits);
    void        append_lower_hex(uint64_t value, uint32_t digits);
    const char *text() const;
    size_t      length() const;

private:
    void append_digits(uint64_t value, uint32_t digits, const char *symbols);

    char  *destination_;
    size_t capacity_;
    size_t length_;
};

}

#endif
