#include "icd/d3d/passthrough_geometry_shader.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::d3d::PassthroughGeometryShader;
using av::d3d::PassthroughPrimitive;

TEST(passthrough_geometry_shader, declares_and_copies_each_register_then_emits_one_point)
{ PassthroughGeometryShader shader(PassthroughPrimitive::point);
  constexpr std::uint32_t position = 1u;
  CHECK(shader.add(0u, 0xFu, position));
  CHECK(shader.add(2u, 0x1u, 0u));
  CHECK(shader.add(2u, 0x2u, 0u));
  const std::uint32_t *const tokens = shader.tokens();
  CHECK(tokens != nullptr);
  if (tokens == nullptr) return;
  const std::uint32_t expected[] = {
      0x00020040u, 36u,
      0x0100085Du, 0x0100085Cu,
      0x05000061u, 0x002010F2u, 1u, 0u, position,
      0x04000067u, 0x001020F2u, 0u, position,
      0x0400005Fu, 0x00201032u, 1u, 2u,
      0x03000065u, 0x00102032u, 2u,
      0x0200005Eu, 1u,
      0x06000036u, 0x001020F2u, 0u, 0x00201E46u, 0u, 0u,
      0x06000036u, 0x00102032u, 2u, 0x00201E46u, 0u, 2u,
      0x01000013u, 0x0100003Eu};
  CHECK_EQ(shader.bytes(), sizeof expected);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < sizeof expected / sizeof expected[0]; index++)
      wrong += tokens[index] == expected[index] ? 0u : 1u;
  CHECK_EQ(wrong, 0u); }

TEST(passthrough_geometry_shader, refuses_a_register_it_cannot_declare)
{ PassthroughGeometryShader shader(PassthroughPrimitive::point);
  CHECK(shader.tokens() == nullptr);
  CHECK(!shader.add(PassthroughGeometryShader::max_registers, 0xFu, 0u));
  CHECK(!shader.add(1u, 0u, 0u));
  CHECK(shader.add(1u, 0x1u, 0u));
  CHECK(!shader.add(1u, 0x2u, 1u));
  CHECK(shader.tokens() != nullptr); }

TEST(passthrough_geometry_shader, every_register_named_fills_the_token_budget_exactly)
{ PassthroughGeometryShader shader(PassthroughPrimitive::point);
  for (std::uint32_t index = 0u; index < PassthroughGeometryShader::max_registers; index++)
      CHECK(shader.add(index, 0xFu, index + 1u));
  const std::uint32_t *const tokens = shader.tokens();
  CHECK(tokens != nullptr);
  constexpr std::uint32_t point_tokens = 8u + PassthroughGeometryShader::max_registers * 15u;
  CHECK_EQ(shader.bytes(), sizeof(std::uint32_t) * point_tokens);
  if (tokens == nullptr) return;
  CHECK_EQ(tokens[1], point_tokens);
  CHECK_EQ(tokens[point_tokens - 1u], 0x0100003Eu); }

TEST(passthrough_geometry_shader, a_triangle_is_read_whole_and_all_three_vertices_are_emitted)
{ PassthroughGeometryShader shader(PassthroughPrimitive::triangle);
  CHECK(shader.add(0u, 0xFu, 0u));
  const std::uint32_t *const tokens = shader.tokens();
  CHECK(tokens != nullptr);
  if (tokens == nullptr) return;
  const std::uint32_t expected[] = {
      0x00020040u, 35u,
      0x0100185Du, 0x0100285Cu,
      0x0400005Fu, 0x002010F2u, 3u, 0u,
      0x03000065u, 0x001020F2u, 0u,
      0x0200005Eu, 3u,
      0x06000036u, 0x001020F2u, 0u, 0x00201E46u, 0u, 0u,
      0x01000013u,
      0x06000036u, 0x001020F2u, 0u, 0x00201E46u, 1u, 0u,
      0x01000013u,
      0x06000036u, 0x001020F2u, 0u, 0x00201E46u, 2u, 0u,
      0x01000013u,
      0x0100003Eu};
  CHECK_EQ(shader.bytes(), sizeof expected);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < sizeof expected / sizeof expected[0]; index++)
      wrong += tokens[index] == expected[index] ? 0u : 1u;
  CHECK_EQ(wrong, 0u); }

TEST(passthrough_geometry_shader, an_adjacency_primitive_emits_the_vertices_its_list_form_has)
{ PassthroughGeometryShader lines(PassthroughPrimitive::line_adjacency);
  PassthroughGeometryShader triangles(PassthroughPrimitive::triangle_adjacency);
  CHECK(lines.add(0u, 0xFu, 0u));
  CHECK(triangles.add(0u, 0xFu, 0u));
  const std::uint32_t *const line_tokens = lines.tokens();
  const std::uint32_t *const triangle_tokens = triangles.tokens();
  CHECK(line_tokens != nullptr && triangle_tokens != nullptr);
  if (line_tokens == nullptr || triangle_tokens == nullptr) return;
  CHECK_EQ(line_tokens[2], 0x0100305Du);
  CHECK_EQ(triangle_tokens[2], 0x0100385Du);
  CHECK_EQ(lines.bytes() + 7u * sizeof(std::uint32_t), triangles.bytes());
  CHECK_EQ(line_tokens[17u], 1u);
  CHECK_EQ(triangle_tokens[17u], 0u); }

TEST(passthrough_geometry_shader, the_input_array_is_declared_as_large_as_the_primitive_it_reads)
{ const PassthroughPrimitive classes[5] = {PassthroughPrimitive::point, PassthroughPrimitive::line,
                                           PassthroughPrimitive::triangle, PassthroughPrimitive::line_adjacency,
                                           PassthroughPrimitive::triangle_adjacency};
  const std::uint32_t        vertices[5] = {1u, 2u, 3u, 4u, 6u};
  for (std::uint32_t which = 0u; which < 5u; which++) {
      PassthroughGeometryShader shader(classes[which]);
      CHECK(shader.add(0u, 0xFu, 0u));
      const std::uint32_t *const tokens = shader.tokens();
      CHECK(tokens != nullptr);
      if (tokens != nullptr) CHECK_EQ(tokens[6], vertices[which]);
  } }
