#include "core/support/text_buffer.hpp"

namespace av {

TextBuffer::TextBuffer(char *destination, size_t capacity)
    : destination_(destination), capacity_(destination != nullptr ? capacity : 0u), length_(0u)
{
    if (capacity_ != 0u) destination_[0] = '\0';
}

void TextBuffer::append(const char *text)
{
    for (; capacity_ != 0u && length_ + 1u < capacity_ && *text != '\0'; text++) destination_[length_++] = *text;
    if (capacity_ != 0u) destination_[length_] = '\0';
}

void TextBuffer::append_decimal(uint64_t value)
{
    char   digits[21] = {};
    size_t first = sizeof digits - 1u;
    do {
        digits[--first] = static_cast<char>('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    append(digits + first);
}

void TextBuffer::append_hex(uint64_t value, uint32_t digits)
{
    constexpr uint32_t widest = 16u;
    constexpr char     symbols[] = "0123456789ABCDEF";
    char               text[widest + 1u] = {};
    const uint32_t     width = digits < widest ? digits : widest;
    for (uint32_t index = width; index != 0u; index--) {
        text[index - 1u] = symbols[value & 0xFu];
        value >>= 4u;
    }
    append(text);
}

const char *TextBuffer::text() const
{
    return capacity_ != 0u ? destination_ : "";
}

size_t TextBuffer::length() const
{
    return length_;
}

}  // namespace av
