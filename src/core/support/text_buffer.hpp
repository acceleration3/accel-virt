/* One NUL-terminated message built in memory the caller owns - the error a
 * realize failure hands back to QEMU, a line of the in-guest report. Text that does not fit is cut, never
 * overrun; a buffer with no room answers the empty string. */
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
    /* The low `digits` hexadecimal digits of `value`, uppercase, at most 16. */
    void        append_hex(uint64_t value, uint32_t digits);
    /* The same in lowercase, which is how Linux spells a PCI address. */
    void        append_lower_hex(uint64_t value, uint32_t digits);
    const char *text() const;
    size_t      length() const;

private:
    void append_digits(uint64_t value, uint32_t digits, const char *symbols);

    char  *destination_;
    size_t capacity_;
    size_t length_;
};

}  // namespace av

#endif  // AV_TEXT_BUFFER_HPP
