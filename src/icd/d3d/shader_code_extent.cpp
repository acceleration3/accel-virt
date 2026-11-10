#include "icd/d3d/shader_code_extent.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

ShaderCodeExtent::ShaderCodeExtent(size_t readable) : readable_(readable)
{
    AV_SCOPE("ShaderCodeExtent::ShaderCodeExtent");
}

size_t ShaderCodeExtent::bytes(const uint32_t *code) const
{
    AV_SCOPE("ShaderCodeExtent::bytes");
    if (code == nullptr || readable_ < sizeof(uint32_t)) return 0u;
    uint64_t declared = 0u;
    if (code[0] == container_magic) {
        if (readable_ < container_header_bytes) return 0u;
        uint32_t total = 0u;
        __builtin_memcpy(&total, reinterpret_cast<const uint8_t *>(code) + container_size_at, sizeof total);
        declared = total;
    } else {
        if (readable_ < token_header_bytes) return 0u;
        declared = uint64_t{code[1]} * sizeof(uint32_t);
    }
    return declared < readable_ ? static_cast<size_t>(declared) : readable_;
}

}
