#include "um/trace/wide_name.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

WideName::WideName() : chars_{}, length_(0u)
{
    AV_SCOPE("WideName::WideName");
}

WideName::WideName(const char *scope, const char *kind, const char *module, uint32_t pid) : WideName()
{
    AV_SCOPE("WideName::WideName");
    append(scope);
    append("\\av-");
    append(kind);
    append("-");
    append(module);
    append("-");
    append(pid);
}

void WideName::append(const char *text)
{
    AV_SCOPE("WideName::append");
    for (; *text != '\0' && length_ + 1u < max_chars; text++) chars_[length_++] = static_cast<wchar_t>(*text);
    chars_[length_] = L'\0';
}

void WideName::append(uint32_t number)
{
    AV_SCOPE("WideName::append");
    char       digits[16];
    TextBuffer decimal(digits, sizeof digits);
    decimal.append_decimal(number);
    append(decimal.text());
}

const wchar_t *WideName::text() const
{
    AV_SCOPE("WideName::text");
    return chars_;
}

}
