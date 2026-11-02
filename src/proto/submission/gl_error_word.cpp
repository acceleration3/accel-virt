#include "proto/submission/gl_error_word.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

constexpr uint64_t published_marker = uint64_t{0x80} << 56u;
constexpr uint32_t bits_per_count = 8u;

}

GlErrorWord::GlErrorWord() : word_(published_marker)
{
    AV_SCOPE("GlErrorWord::GlErrorWord");
}

GlErrorWord::GlErrorWord(uint64_t word) : word_(word)
{
    AV_SCOPE("GlErrorWord::GlErrorWord");
}

void GlErrorWord::record(uint32_t error)
{
    AV_SCOPE("GlErrorWord::record");
    if (error < first_error || error - first_error >= kinds) return;
    const uint32_t shift = bits_per_count * (error - first_error);
    const uint64_t counted = static_cast<uint8_t>(count(error - first_error) + 1u);
    word_ = (word_ & ~(uint64_t{0xFF} << shift)) | (counted << shift);
}

bool GlErrorWord::published() const
{
    AV_SCOPE("GlErrorWord::published");
    return (word_ & published_marker) != 0u;
}

uint8_t GlErrorWord::count(uint32_t kind) const
{
    AV_SCOPE("GlErrorWord::count");
    if (kind >= kinds) return 0u;
    return static_cast<uint8_t>(word_ >> (bits_per_count * kind));
}

uint64_t GlErrorWord::word() const
{
    AV_SCOPE("GlErrorWord::word");
    return word_;
}

}
