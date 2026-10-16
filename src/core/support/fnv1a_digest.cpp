#include "core/support/fnv1a_digest.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

Fnv1aDigest::Fnv1aDigest() : value_(offset_basis)
{
    AV_SCOPE("Fnv1aDigest::Fnv1aDigest");
}

Fnv1aDigest::Fnv1aDigest(uint64_t salt) : value_(offset_basis ^ salt)
{
    AV_SCOPE("Fnv1aDigest::Fnv1aDigest");
}

void Fnv1aDigest::add(const void *bytes, size_t count)
{
    AV_SCOPE("Fnv1aDigest::add");
    if (bytes == nullptr) return;
    const uint8_t *const from = static_cast<const uint8_t *>(bytes);
    for (size_t index = 0u; index < count; ++index) value_ = (value_ ^ from[index]) * prime;
}

void Fnv1aDigest::add_text(const char *text)
{
    AV_SCOPE("Fnv1aDigest::add_text");
    size_t length = 0u;
    while (text != nullptr && text[length] != '\0') ++length;
    add(text, length);
    const uint8_t terminator = 0u;
    add(&terminator, 1u);
}

void Fnv1aDigest::add_word(uint64_t word)
{
    AV_SCOPE("Fnv1aDigest::add_word");
    value_ = (value_ ^ word) * prime;
}

uint64_t Fnv1aDigest::value() const
{
    AV_SCOPE("Fnv1aDigest::value");
    return value_;
}

}  // namespace av
