#include "device/support/text_buffer.hpp"

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

const char *TextBuffer::text() const
{
    return capacity_ != 0u ? destination_ : "";
}

size_t TextBuffer::length() const
{
    return length_;
}

}  // namespace av
