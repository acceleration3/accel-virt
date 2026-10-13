/* The geometry shader a stream-output declaration with no shader stands for
 * (D3D11DDIARG_CREATEGEOMETRYSHADERWITHSTREAMOUTPUT with a null pShaderCode):
 * the vertex shader's output streamed as it is. The runtime hands no code then,
 * only the registers, so this writes the shader-model 4 token stream of one -
 * a point in, the same point out, every register copied - which the host
 * compiles as it would an application's, inside the container it builds from
 * the same signatures. A register carrying a system value, the position above
 * all, is declared as one: a host reads it as that value, not as a varying. */
#ifndef AV_PASSTHROUGH_GEOMETRY_SHADER_HPP
#define AV_PASSTHROUGH_GEOMETRY_SHADER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

class PassthroughGeometryShader {
public:
    /* As many as a stage's output signature can name. */
    static constexpr uint32_t max_registers = 32u;
    /* Every token one register costs, and the shader's own. */
    static constexpr uint32_t max_tokens = 8u + max_registers * 15u;

    PassthroughGeometryShader();

    /* Register `index`'s components `mask` passed through, as the system value
     * `name` (a D3D10_SB_NAME, 0 for none). A register named again - elements
     * packed into one - passes the union of its components. False for a
     * register past max_registers, no components, or a register two system
     * values claim. */
    [[nodiscard]] bool add(uint32_t index, uint32_t mask, uint32_t name);
    /* The token stream; null with no register added. */
    const uint32_t *tokens();
    /* Its length in bytes. */
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

}  // namespace av::d3d

#endif  // AV_PASSTHROUGH_GEOMETRY_SHADER_HPP
