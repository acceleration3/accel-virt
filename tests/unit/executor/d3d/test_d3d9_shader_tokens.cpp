#include "executor/d3d/d3d9_shader_tokens.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>
#include <cstring>

using av::D3d9ShaderTokens;

namespace {

constexpr std::uint32_t end = 0x0000ffffu;
constexpr std::uint32_t vs_2_0 = 0xfffe0200u;
constexpr std::uint32_t vs_3_0 = 0xfffe0300u;
constexpr std::uint32_t ps_2_0 = 0xffff0200u;
constexpr std::uint32_t ps_1_4 = 0xffff0104u;
constexpr std::uint32_t vs_1_1 = 0xfffe0101u;
/* mov oPos, v0 at shader model 2: opcode 1, two parameters, so length 2. */
constexpr std::uint32_t mov = 0x02000001u;
constexpr std::uint32_t dst_opos = 0xc00f0000u;
constexpr std::uint32_t src_v0 = 0x90e40000u;
/* A comment of two dwords. */
constexpr std::uint32_t comment_2 = 0x0002fffeu;

bool judged(const std::uint32_t *tokens, std::size_t count, bool &vertex)
{
    std::uint8_t bytes[64 * 4] = {};
    std::memcpy(bytes, tokens, count * 4u);
    const D3d9ShaderTokens code(bytes, count * 4u);
    vertex = code.vertex();
    return code.valid();
}

}  // namespace

TEST(d3d9_shader_tokens, accepts_a_shader_each_model_writes)
{
    bool vertex = false;
    const std::uint32_t vs2[] = {vs_2_0, mov, dst_opos, src_v0, end};
    CHECK(judged(vs2, 5u, vertex));
    CHECK(vertex);
    const std::uint32_t vs3[] = {vs_3_0, comment_2, 0x41414141u, 0x42424242u, mov, dst_opos, src_v0, end};
    CHECK(judged(vs3, 8u, vertex));
    CHECK(vertex);
    const std::uint32_t ps2[] = {ps_2_0, end};
    CHECK(judged(ps2, 2u, vertex));
    CHECK(!vertex);
    /* Shader model 1 says no instruction's length; its end is all there is to judge. */
    const std::uint32_t ps14[] = {ps_1_4, 0x00000001u, 0x800f0000u, 0xb0e40000u, end};
    CHECK(judged(ps14, 5u, vertex));
    const std::uint32_t vs11[] = {vs_1_1, end};
    CHECK(judged(vs11, 2u, vertex));
    CHECK(vertex);
}

TEST(d3d9_shader_tokens, refuses_what_is_not_a_whole_shader)
{
    bool vertex = false;
    /* No end token; the end token before the last dword; nothing but a version. */
    const std::uint32_t unterminated[] = {vs_2_0, mov, dst_opos, src_v0};
    CHECK(!judged(unterminated, 4u, vertex));
    const std::uint32_t early_end[] = {vs_2_0, end, mov, dst_opos, src_v0, end};
    CHECK(!judged(early_end, 6u, vertex));
    const std::uint32_t version_only[] = {vs_2_0};
    CHECK(!judged(version_only, 1u, vertex));
    /* An instruction or a comment that runs past the end. */
    const std::uint32_t long_instruction[] = {vs_2_0, 0x0f000001u, dst_opos, end};
    CHECK(!judged(long_instruction, 4u, vertex));
    const std::uint32_t long_comment[] = {ps_2_0, 0x0010fffeu, end};
    CHECK(!judged(long_comment, 3u, vertex));
    /* Versions nobody writes: model 4, a minor version past what 2 has, no type. */
    const std::uint32_t model_4[] = {0xfffe0400u, end};
    CHECK(!judged(model_4, 2u, vertex));
    const std::uint32_t ps_2_2[] = {0xffff0202u, end};
    CHECK(!judged(ps_2_2, 2u, vertex));
    const std::uint32_t untyped[] = {0x00000200u, end};
    CHECK(!judged(untyped, 2u, vertex));
}

TEST(d3d9_shader_tokens, refuses_a_length_that_is_not_whole_dwords)
{
    const std::uint32_t ps2[] = {ps_2_0, end};
    std::uint8_t bytes[9] = {};
    std::memcpy(bytes, ps2, sizeof(ps2));
    CHECK(!D3d9ShaderTokens(bytes, 7u).valid());
    CHECK(!D3d9ShaderTokens(bytes, 9u).valid());
    CHECK(!D3d9ShaderTokens(bytes, 0u).valid());
    CHECK(!D3d9ShaderTokens(nullptr, 8u).valid());
    CHECK(D3d9ShaderTokens(bytes, 8u).valid());
    CHECK_EQ(D3d9ShaderTokens(bytes, 8u).dwords(), 2u);
}

/* Every single byte flipped in a valid shader is judged without reading past
 * it: what is refused or kept matters less than that nothing is read wrongly. */
TEST(d3d9_shader_tokens, survives_every_byte_flipped)
{
    const std::uint32_t vs3[] = {vs_3_0, comment_2, 0x41414141u, 0x42424242u, mov, dst_opos, src_v0, end};
    for (std::size_t at = 0u; at < sizeof(vs3); at++) {
        std::uint8_t bytes[sizeof(vs3)];
        std::memcpy(bytes, vs3, sizeof(vs3));
        bytes[at] = static_cast<std::uint8_t>(bytes[at] ^ 0xffu);
        const D3d9ShaderTokens code(bytes, sizeof(bytes));
        if (code.valid()) CHECK_EQ(code.dwords(), 8u);
    }
}
