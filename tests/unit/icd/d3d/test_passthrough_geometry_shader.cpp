/* The pass-through geometry shader's token stream, word for word: a point in,
 * the same point out, a system value declared as one, every register copied,
 * one vertex emitted - elements packed into one register passed as one, and
 * nothing for no register, or one two system values claim. */
#include "icd/d3d/passthrough_geometry_shader.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::d3d::PassthroughGeometryShader;

TEST(passthrough_geometry_shader, declares_and_copies_each_register_then_emits_one_point)
{ PassthroughGeometryShader shader;
  constexpr std::uint32_t position = 1u;
  CHECK(shader.add(0u, 0xFu, position));
  CHECK(shader.add(2u, 0x1u, 0u));
  CHECK(shader.add(2u, 0x2u, 0u));   /* packed beside it: .xy in all */
  const std::uint32_t *const tokens = shader.tokens();
  CHECK(tokens != nullptr);
  if (tokens == nullptr) return;
  const std::uint32_t expected[] = {
      0x00020040u, 36u,                                       /* gs_4_0, its length */
      0x0100085Du, 0x0100085Cu,                               /* point in, pointlist out */
      0x05000061u, 0x002010F2u, 1u, 0u, position,             /* dcl_input_siv v[1][0].xyzw, position */
      0x04000067u, 0x001020F2u, 0u, position,                 /* dcl_output_siv o0.xyzw, position */
      0x0400005Fu, 0x00201032u, 1u, 2u,                       /* dcl_input v[1][2].xy */
      0x03000065u, 0x00102032u, 2u,                           /* dcl_output o2.xy */
      0x0200005Eu, 1u,                                        /* dcl_maxout 1 */
      0x06000036u, 0x001020F2u, 0u, 0x00201E46u, 0u, 0u,      /* mov o0.xyzw, v[0][0].xyzw */
      0x06000036u, 0x00102032u, 2u, 0x00201E46u, 0u, 2u,      /* mov o2.xy, v[0][2].xyzw */
      0x01000013u, 0x0100003Eu};                              /* emit, ret */
  CHECK_EQ(shader.bytes(), sizeof expected);
  std::uint32_t wrong = 0u;
  for (std::uint32_t index = 0u; index < sizeof expected / sizeof expected[0]; index++)
      wrong += tokens[index] == expected[index] ? 0u : 1u;
  CHECK_EQ(wrong, 0u); }

TEST(passthrough_geometry_shader, refuses_a_register_it_cannot_declare)
{ PassthroughGeometryShader shader;
  CHECK(shader.tokens() == nullptr);
  CHECK(!shader.add(PassthroughGeometryShader::max_registers, 0xFu, 0u));
  CHECK(!shader.add(1u, 0u, 0u));
  CHECK(shader.add(1u, 0x1u, 0u));
  CHECK(!shader.add(1u, 0x2u, 1u));   /* a system value where an element is */
  CHECK(shader.tokens() != nullptr); }
