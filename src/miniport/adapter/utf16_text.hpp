/* A short UTF-8 text as the UTF-16 Windows stores device properties and
 * registry strings in: NUL-terminated in a fixed buffer, cut at a character
 * boundary when it does not fit. A byte sequence that is not UTF-8 becomes
 * U+FFFD, one per byte, so text from a peer cannot make it anything but a
 * well-formed string. */
#ifndef AV_UTF16_TEXT_HPP
#define AV_UTF16_TEXT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av {

class Utf16Text {
public:
    /* Code units, the NUL among them. */
    static constexpr size_t capacity = 64u;

    explicit Utf16Text(const char *utf8);

    const uint16_t *units() const;
    /* The bytes Windows is handed: every unit and the NUL. */
    uint32_t        bytes() const;
    /* Whether there is nothing before the NUL. */
    bool            empty() const;

private:
    static constexpr uint16_t replacement = 0xFFFDu;

    /* The code point starting at `utf8`, and how many bytes it took: at least
     * one, so a bad byte is consumed as U+FFFD. */
    uint32_t decode(const unsigned char *utf8, size_t &taken) const;
    /* Whether the code point fits before the NUL: false leaves it out. */
    bool     append(uint32_t code_point);

    uint16_t units_[capacity];
    size_t   length_;
};

}  // namespace av

#endif  // AV_UTF16_TEXT_HPP
