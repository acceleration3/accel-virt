#include "executor/d3d/d3d9_shader_tokens.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av {

namespace {

constexpr uint32_t end_token = 0x0000ffffu;
constexpr uint32_t comment_opcode = 0xfffeu;
constexpr uint32_t vertex_type = 0xfffeu;
constexpr uint32_t pixel_type = 0xffffu;
constexpr uint32_t type_shift = 16u;
constexpr uint32_t major_shift = 8u;
constexpr uint32_t byte_mask = 0xffu;
constexpr uint32_t opcode_mask = 0xffffu;
constexpr uint32_t comment_length_shift = 16u;
constexpr uint32_t comment_length_mask = 0x7fffu;
constexpr uint32_t length_shift = 24u;
constexpr uint32_t length_mask = 0xfu;
constexpr uint32_t dword_bytes = 4u;
constexpr uint32_t walked_from_major = 2u;

}

D3d9ShaderTokens::D3d9ShaderTokens(const uint8_t *code, uint64_t size)
    : code_(code), dwords_(size / dword_bytes), valid_(false)
{
    AV_SCOPE("D3d9ShaderTokens::D3d9ShaderTokens");
    if (code == nullptr || size % dword_bytes != 0u) return;
    valid_ = judge();
}

bool D3d9ShaderTokens::valid() const
{
    AV_SCOPE("D3d9ShaderTokens::valid");
    return valid_;
}

bool D3d9ShaderTokens::vertex() const
{
    AV_SCOPE("D3d9ShaderTokens::vertex");
    return valid_ && at(0u) >> type_shift == vertex_type;
}

uint64_t D3d9ShaderTokens::dwords() const
{
    AV_SCOPE("D3d9ShaderTokens::dwords");
    return dwords_;
}

uint32_t D3d9ShaderTokens::at(uint64_t index) const
{
    AV_SCOPE("D3d9ShaderTokens::at");
    uint32_t value = 0u;
    memcpy(&value, code_ + index * dword_bytes, sizeof(value));
    return value;
}

bool D3d9ShaderTokens::judge() const
{
    AV_SCOPE("D3d9ShaderTokens::judge");
    constexpr uint64_t smallest = 2u;
    if (dwords_ < smallest || at(dwords_ - 1u) != end_token || !version_known(at(0u))) return false;
    if ((at(0u) >> major_shift & byte_mask) < walked_from_major) {
        for (uint64_t index = 1u; index + 1u < dwords_; index++)
            if (at(index) == end_token) return false;
        return true;
    }
    return walks_to_end();
}

bool D3d9ShaderTokens::version_known(uint32_t version) const
{
    AV_SCOPE("D3d9ShaderTokens::version_known");
    const uint32_t type = version >> type_shift;
    const uint32_t major = version >> major_shift & byte_mask;
    const uint32_t minor = version & byte_mask;
    if (type != vertex_type && type != pixel_type) return false;
    switch (major) {
    case 1u: return type == vertex_type ? minor == 1u : minor >= 1u && minor <= 4u;
    case 2u: return minor <= 1u;
    case 3u: return minor == 0u;
    default: return false;
    }
}

bool D3d9ShaderTokens::walks_to_end() const
{
    AV_SCOPE("D3d9ShaderTokens::walks_to_end");
    uint64_t index = 1u;
    while (index + 1u < dwords_) {
        const uint32_t token = at(index);
        if (token == end_token) return false;
        const uint64_t length = (token & opcode_mask) == comment_opcode
                                    ? token >> comment_length_shift & comment_length_mask
                                    : token >> length_shift & length_mask;
        index += 1u + length;
    }
    return index + 1u == dwords_;
}

}
