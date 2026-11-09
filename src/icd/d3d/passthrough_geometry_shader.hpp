#ifndef AV_PASSTHROUGH_GEOMETRY_SHADER_HPP
#define AV_PASSTHROUGH_GEOMETRY_SHADER_HPP

#include <stddef.h>
#include <stdint.h>

namespace av::d3d {

enum class PassthroughPrimitive : uint8_t { point, line, triangle, line_adjacency, triangle_adjacency };

class PassthroughGeometryShader {
public:
    static constexpr uint32_t max_registers = 32u;
    static constexpr uint32_t max_vertices = 3u;
    static constexpr uint32_t max_tokens = 8u + max_registers * (9u + 6u * max_vertices) + (max_vertices - 1u);

    explicit PassthroughGeometryShader(PassthroughPrimitive primitive);

    [[nodiscard]] bool add(uint32_t index, uint32_t mask, uint32_t name);
    const uint32_t *tokens();
    size_t          bytes() const;

private:
    void put(uint32_t token);
    void declare_input(uint32_t index);
    void declare_output(uint32_t index);
    void copy(uint32_t index, uint32_t vertex);

    uint32_t input_;
    uint32_t input_vertices_;
    uint32_t output_;
    uint32_t emitted_;
    uint32_t source_[max_vertices];
    uint32_t masks_[max_registers];
    uint32_t names_[max_registers];
    uint32_t tokens_[max_tokens];
    uint32_t count_;
};

}

#endif
