#ifndef AV_D3D_SHADER_CODE_EXTENT_HPP
#define AV_D3D_SHADER_CODE_EXTENT_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class ShaderCodeExtent {
public:
    static constexpr uint32_t container_magic = 0x43425844u;
    static constexpr size_t   container_size_at = 24u;
    static constexpr size_t   container_header_bytes = 28u;
    static constexpr size_t   token_header_bytes = 8u;

    explicit ShaderCodeExtent(size_t readable);

    size_t bytes(const uint32_t *code) const;

private:
    size_t readable_;
};

}

#endif
