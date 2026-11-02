#include "miniport/adapter/utf16_text.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint32_t continuation_mask = 0xC0u;
constexpr uint32_t continuation_bits = 0x80u;
constexpr uint32_t payload_mask      = 0x3Fu;
constexpr uint32_t highest_code_point = 0x10FFFFu;
constexpr uint32_t first_surrogate   = 0xD800u;
constexpr uint32_t last_surrogate    = 0xDFFFu;
constexpr uint32_t first_supplementary = 0x10000u;
constexpr uint32_t high_surrogate    = 0xD800u;
constexpr uint32_t low_surrogate     = 0xDC00u;
constexpr uint32_t surrogate_bits    = 10u;
constexpr uint32_t surrogate_mask    = 0x3FFu;

}

Utf16Text::Utf16Text(const char *utf8) : units_{}, length_(0u)
{
    AV_SCOPE("Utf16Text::Utf16Text");
    const unsigned char *cursor = static_cast<const unsigned char *>(static_cast<const void *>(utf8));
    while (*cursor != 0u) {
        size_t         taken = 0u;
        const uint32_t code_point = decode(cursor, taken);
        if (!append(code_point)) break;
        cursor += taken;
    }
}

const uint16_t *Utf16Text::units() const
{
    AV_SCOPE("Utf16Text::units");
    return units_;
}

uint32_t Utf16Text::bytes() const
{
    AV_SCOPE("Utf16Text::bytes");
    return static_cast<uint32_t>((length_ + 1u) * sizeof(uint16_t));
}

bool Utf16Text::empty() const
{
    AV_SCOPE("Utf16Text::empty");
    return length_ == 0u;
}

uint32_t Utf16Text::decode(const unsigned char *utf8, size_t &taken) const
{
    AV_SCOPE("Utf16Text::decode");
    taken = 1u;
    const uint32_t lead = utf8[0];
    if (lead < continuation_bits) return lead;
    size_t   trailing = 0u;
    uint32_t minimum = 0u;
    uint32_t code_point = 0u;
    if (lead >= 0xC2u && lead <= 0xDFu) {
        trailing = 1u, minimum = 0x80u, code_point = lead & 0x1Fu;
    } else if (lead >= 0xE0u && lead <= 0xEFu) {
        trailing = 2u, minimum = 0x800u, code_point = lead & 0x0Fu;
    } else if (lead >= 0xF0u && lead <= 0xF4u) {
        trailing = 3u, minimum = first_supplementary, code_point = lead & 0x07u;
    } else {
        return replacement;
    }
    for (size_t index = 1u; index <= trailing; index++) {
        if ((utf8[index] & continuation_mask) != continuation_bits) return replacement;
        code_point = code_point << 6u | (utf8[index] & payload_mask);
    }
    if (code_point < minimum || code_point > highest_code_point ||
        (code_point >= first_surrogate && code_point <= last_surrogate))
        return replacement;
    taken = 1u + trailing;
    return code_point;
}

bool Utf16Text::append(uint32_t code_point)
{
    AV_SCOPE("Utf16Text::append");
    const size_t needed = code_point >= first_supplementary ? 2u : 1u;
    if (length_ + needed >= capacity) return false;
    if (needed == 1u) {
        units_[length_++] = static_cast<uint16_t>(code_point);
        return true;
    }
    const uint32_t offset = code_point - first_supplementary;
    units_[length_++] = static_cast<uint16_t>(high_surrogate + (offset >> surrogate_bits));
    units_[length_++] = static_cast<uint16_t>(low_surrogate + (offset & surrogate_mask));
    return true;
}

}
