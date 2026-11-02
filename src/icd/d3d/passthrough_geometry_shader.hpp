#ifndef AV_PASSTHROUGH_GEOMETRY_SHADER_HPP
#define AV_PASSTHROUGH_GEOMETRY_SHADER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class PassthroughGeometryShader {
public:
    static constexpr uint32_t max_registers = 32u;
    static constexpr uint32_t max_tokens = 8u + max_registers * 15u;

    PassthroughGeometryShader();

    [[nodiscard]] bool add(uint32_t index, uint32_t mask, uint32_t name);
    const uint32_t *tokens();
    size_t          bytes() const;

private:
    void put(uint32_t token);
    void declare_input(uint32_t index);
    void declare_output(uint32_t index);
    void copy(uint32_t index);

    uint32_t masks_[max_registers];
    uint32_t names_[max_registers];
    uint32_t tokens_[max_tokens];
    uint32_t count_;
};

}

#endif
