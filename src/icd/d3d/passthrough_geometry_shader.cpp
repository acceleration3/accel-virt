#include "icd/d3d/passthrough_geometry_shader.hpp"

#include "core/trace/function_scope.hpp"

namespace av::d3d {

namespace {
constexpr uint32_t geometry_shader_4_0 = (2u << 16u) | (4u << 4u);
constexpr uint32_t op_mov = 0x36u;
constexpr uint32_t op_ret = 0x3Eu;
constexpr uint32_t op_dcl_output_topology = 0x5Cu;
constexpr uint32_t op_dcl_input_primitive = 0x5Du;
constexpr uint32_t op_dcl_max_vertices = 0x5Eu;
constexpr uint32_t op_dcl_input = 0x5Fu;
constexpr uint32_t op_dcl_input_siv = 0x61u;
constexpr uint32_t op_dcl_output = 0x65u;
constexpr uint32_t op_dcl_output_siv = 0x67u;
constexpr uint32_t op_emit = 0x13u;
constexpr uint32_t point = 1u;
constexpr uint32_t line = 2u;
constexpr uint32_t line_strip = 3u;
constexpr uint32_t triangle = 3u;
constexpr uint32_t triangle_strip = 5u;
constexpr uint32_t line_adjacency = 6u;
constexpr uint32_t triangle_adjacency = 7u;
constexpr uint32_t four_components = 2u;
constexpr uint32_t swizzle_mode = 1u << 2u;
constexpr uint32_t xyzw = 0xE4u;
constexpr uint32_t type_input = 1u << 12u;
constexpr uint32_t type_output = 2u << 12u;
constexpr uint32_t one_index = 1u << 20u;
constexpr uint32_t two_indices = 2u << 20u;

constexpr uint32_t opcode(uint32_t code, uint32_t length) { return code | (length << 24u); }

}

PassthroughGeometryShader::PassthroughGeometryShader(PassthroughPrimitive primitive)
    : input_(point), output_(point), emitted_(1u), source_{0u, 0u, 0u}, masks_{}, names_{}, tokens_{}, count_(0u)
{
    AV_SCOPE("PassthroughGeometryShader::PassthroughGeometryShader");
    switch (primitive) {
    case PassthroughPrimitive::point: break;
    case PassthroughPrimitive::line:
        input_ = line;
        output_ = line_strip;
        emitted_ = 2u;
        source_[1] = 1u;
        break;
    case PassthroughPrimitive::triangle:
        input_ = triangle;
        output_ = triangle_strip;
        emitted_ = 3u;
        source_[1] = 1u;
        source_[2] = 2u;
        break;
    case PassthroughPrimitive::line_adjacency:
        input_ = line_adjacency;
        output_ = line_strip;
        emitted_ = 2u;
        source_[0] = 1u;
        source_[1] = 2u;
        break;
    case PassthroughPrimitive::triangle_adjacency:
        input_ = triangle_adjacency;
        output_ = triangle_strip;
        emitted_ = 3u;
        source_[1] = 2u;
        source_[2] = 4u;
        break;
    }
}

bool PassthroughGeometryShader::add(uint32_t index, uint32_t mask, uint32_t name)
{
    AV_SCOPE("PassthroughGeometryShader::add");
    if (index >= max_registers || (mask & 0xFu) == 0u) return false;
    if (masks_[index] != 0u && names_[index] != name) return false;
    masks_[index] |= mask & 0xFu;
    names_[index] = name;
    return true;
}

const uint32_t *PassthroughGeometryShader::tokens()
{
    AV_SCOPE("PassthroughGeometryShader::tokens");
    count_ = 0u;
    put(geometry_shader_4_0);
    put(0u);
    put(opcode(op_dcl_input_primitive, 1u) | (input_ << 11u));
    put(opcode(op_dcl_output_topology, 1u) | (output_ << 11u));
    bool any = false;
    for (uint32_t index = 0u; index < max_registers; index++) {
        if (masks_[index] == 0u) continue;
        any = true;
        declare_input(index);
        declare_output(index);
    }
    put(opcode(op_dcl_max_vertices, 2u));
    put(emitted_);
    for (uint32_t vertex = 0u; vertex < emitted_; vertex++) {
        for (uint32_t index = 0u; index < max_registers; index++)
            if (masks_[index] != 0u) copy(index, source_[vertex]);
        put(opcode(op_emit, 1u));
    }
    put(opcode(op_ret, 1u));
    tokens_[1] = count_;
    return any ? tokens_ : nullptr;
}

size_t PassthroughGeometryShader::bytes() const
{
    AV_SCOPE("PassthroughGeometryShader::bytes");
    return size_t{count_} * sizeof(uint32_t);
}

void PassthroughGeometryShader::put(uint32_t token)
{
    AV_SCOPE("PassthroughGeometryShader::put");
    tokens_[count_++] = token;
}

void PassthroughGeometryShader::declare_input(uint32_t index)
{
    AV_SCOPE("PassthroughGeometryShader::declare_input");
    const bool named = names_[index] != 0u;
    put(opcode(named ? op_dcl_input_siv : op_dcl_input, named ? 5u : 4u));
    put(four_components | (masks_[index] << 4u) | type_input | two_indices);
    put(1u);
    put(index);
    if (named) put(names_[index]);
}

void PassthroughGeometryShader::declare_output(uint32_t index)
{
    AV_SCOPE("PassthroughGeometryShader::declare_output");
    const bool named = names_[index] != 0u;
    put(opcode(named ? op_dcl_output_siv : op_dcl_output, named ? 4u : 3u));
    put(four_components | (masks_[index] << 4u) | type_output | one_index);
    put(index);
    if (named) put(names_[index]);
}

void PassthroughGeometryShader::copy(uint32_t index, uint32_t vertex)
{
    AV_SCOPE("PassthroughGeometryShader::copy");
    put(opcode(op_mov, 6u));
    put(four_components | (masks_[index] << 4u) | type_output | one_index);
    put(index);
    put(four_components | swizzle_mode | (xyzw << 4u) | type_input | two_indices);
    put(vertex);
    put(index);
}

}
