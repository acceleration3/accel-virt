/* A Direct3D 9 shader's token stream, as a guest sent it, judged before DXVK
 * reads it: whole dwords, a vertex or pixel version a model 1-3 shader has,
 * and an end token that is the last dword. From shader model 2 on, every
 * instruction says its own length, so the stream is walked instruction by
 * instruction and none may run past the end; model 1's lengths are only in
 * the opcode tables, so there the end token is all there is to judge - which
 * is why the executor copies a stream into memory with ends past it before
 * DXVK reads it. */
#ifndef AV_D3D9_SHADER_TOKENS_HPP
#define AV_D3D9_SHADER_TOKENS_HPP

#include <stdint.h>

namespace av {

class D3d9ShaderTokens {
public:
    D3d9ShaderTokens(const uint8_t *code, uint64_t size);

    bool     valid() const;
    /* A vertex shader's version; false for a pixel shader or no shader. */
    bool     vertex() const;
    uint64_t dwords() const;

private:
    /* The dword at `index`, read without assuming alignment. */
    uint32_t at(uint64_t index) const;
    bool     judge() const;
    bool     version_known(uint32_t version) const;
    /* Model 2 and 3's instructions and comments end where they say, all before the last dword. */
    bool     walks_to_end() const;

    const uint8_t *code_;
    uint64_t       dwords_;
    bool           valid_;
};

}  // namespace av

#endif  // AV_D3D9_SHADER_TOKENS_HPP
