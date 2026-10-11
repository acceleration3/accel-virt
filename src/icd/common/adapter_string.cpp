#include "icd/common/adapter_string.hpp"

#include "core/trace/function_scope.hpp"

namespace av::icd {

AdapterString::AdapterString() : text_{}
{
    AV_SCOPE("AdapterString::AdapterString");
}

void AdapterString::take(const uint16_t *wide)
{
    AV_SCOPE("AdapterString::take");
    size_t used = 0u;
    for (size_t index = 0u; wide != nullptr && wide[index] != 0u; ++index) {
        uint32_t       code = wide[index];
        const uint32_t next = wide[index + 1u];
        if (code >= 0xD800u && code <= 0xDBFFu && next >= 0xDC00u && next <= 0xDFFFu) {
            code = 0x10000u + ((code - 0xD800u) << 10u) + (next - 0xDC00u);
            ++index;
        } else if (code >= 0xD800u && code <= 0xDFFFu) {
            code = 0xFFFDu;
        }
        if (!append(code, used)) break;
    }
    text_[used] = '\0';
}

bool AdapterString::append(uint32_t code, size_t &used)
{
    AV_SCOPE("AdapterString::append");
    uint8_t encoded[4] = {};
    size_t  length = 0u;
    if (code < 0x80u) {
        encoded[length++] = static_cast<uint8_t>(code);
    } else if (code < 0x800u) {
        encoded[length++] = static_cast<uint8_t>(0xC0u | (code >> 6u));
        encoded[length++] = static_cast<uint8_t>(0x80u | (code & 0x3Fu));
    } else if (code < 0x10000u) {
        encoded[length++] = static_cast<uint8_t>(0xE0u | (code >> 12u));
        encoded[length++] = static_cast<uint8_t>(0x80u | ((code >> 6u) & 0x3Fu));
        encoded[length++] = static_cast<uint8_t>(0x80u | (code & 0x3Fu));
    } else {
        encoded[length++] = static_cast<uint8_t>(0xF0u | (code >> 18u));
        encoded[length++] = static_cast<uint8_t>(0x80u | ((code >> 12u) & 0x3Fu));
        encoded[length++] = static_cast<uint8_t>(0x80u | ((code >> 6u) & 0x3Fu));
        encoded[length++] = static_cast<uint8_t>(0x80u | (code & 0x3Fu));
    }
    if (used + length + 1u > bytes) return false;
    for (size_t at = 0u; at < length; ++at) text_[used++] = static_cast<char>(encoded[at]);
    return true;
}

const char *AdapterString::text() const
{
    AV_SCOPE("AdapterString::text");
    return text_;
}

}  // namespace av::icd
