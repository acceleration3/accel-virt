/* One NUL-terminated message built in memory the caller owns - the error a
 * realize failure hands back to QEMU. Text that does not fit is cut, never
 * overrun; a buffer with no room answers the empty string. */
#ifndef AV_TEXT_BUFFER_HPP
#define AV_TEXT_BUFFER_HPP

#include <stddef.h>

namespace av {

class TextBuffer {
public:
    TextBuffer(char *destination, size_t capacity);
    TextBuffer(const TextBuffer &) = delete;
    TextBuffer &operator=(const TextBuffer &) = delete;

    void        append(const char *text);
    const char *text() const;
    size_t      length() const;

private:
    char  *destination_;
    size_t capacity_;
    size_t length_;
};

}  // namespace av

#endif  // AV_TEXT_BUFFER_HPP
