#ifndef AV_D3D9_SHADER_TOKENS_HPP
#define AV_D3D9_SHADER_TOKENS_HPP

#include <stdint.h>

namespace av {

class D3d9ShaderTokens {
public:
    D3d9ShaderTokens(const uint8_t *code, uint64_t size);

    bool     valid() const;
    bool     vertex() const;
    uint64_t dwords() const;

private:
    uint32_t at(uint64_t index) const;
    bool     judge() const;
    bool     version_known(uint32_t version) const;
    bool     walks_to_end() const;

    const uint8_t *code_;
    uint64_t       dwords_;
    bool           valid_;
};

}

#endif
