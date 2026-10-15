#include "executor/opencl/cl_binary_digest.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

ClBinaryDigest::ClBinaryDigest() : value_(offset_basis)
{
    AV_SCOPE("ClBinaryDigest::ClBinaryDigest");
}

void ClBinaryDigest::add(const void *bytes, size_t count)
{
    AV_SCOPE("ClBinaryDigest::add");
    if (bytes == nullptr) return;
    const uint8_t *const from = static_cast<const uint8_t *>(bytes);
    for (size_t index = 0u; index < count; ++index) value_ = (value_ ^ from[index]) * prime;
}

void ClBinaryDigest::add_text(const char *text)
{
    AV_SCOPE("ClBinaryDigest::add_text");
    size_t length = 0u;
    while (text != nullptr && text[length] != '\0') ++length;
    add(text, length);
    const uint8_t terminator = 0u;
    add(&terminator, 1u);
}

uint64_t ClBinaryDigest::value() const
{
    AV_SCOPE("ClBinaryDigest::value");
    return value_;
}

}  // namespace av
