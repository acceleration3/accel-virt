/* How many values a uniform location answers, from its type (OpenGL 4.6 table
 * 7.3), kept per program and location until the program is linked again or
 * deleted. */
#include "harness/framework/test_registry.hpp"
#include "icd/opengl/uniform_shape.hpp"
#include "icd/opengl/uniform_shapes.hpp"

#include <cstdint>

using av::glicd::UniformShape;
using av::glicd::UniformShapes;

namespace av::glicd {

struct UniformShapesProbe {
    static void overfill(UniformShapes &shapes) { shapes.count_ = UniformShapes::capacity + 1u; }
    static void negative(UniformShapes &shapes) { shapes.shapes_[0] = UniformShape(); shapes.count_ = 1u; }
    static void unshaped(UniformShapes &shapes) { shapes.shapes_[0] = UniformShape(1u, 0, 0x1u); shapes.count_ = 1u; }
    static void repeat(UniformShapes &shapes)
    {
        shapes.shapes_[0] = UniformShape(1u, 2, 0x1406u);
        shapes.shapes_[1] = UniformShape(1u, 2, 0x1406u);
        shapes.count_ = 2u;
    }
};

}  // namespace av::glicd

TEST(uniform_shape, a_type_names_its_value_count)
{ struct Row { std::uint32_t type; std::uint32_t values; };
  const Row rows[] = {
      {0x1406u, 1u},  {0x8B50u, 2u},  {0x8B51u, 3u},  {0x8B52u, 4u},  {0x1404u, 1u},  {0x8B53u, 2u},
      {0x8B54u, 3u},  {0x8B55u, 4u},  {0x1405u, 1u},  {0x8DC6u, 2u},  {0x8DC7u, 3u},  {0x8DC8u, 4u},
      {0x8B56u, 1u},  {0x8B57u, 2u},  {0x8B58u, 3u},  {0x8B59u, 4u},  {0x8B5Au, 4u},  {0x8B5Bu, 9u},
      {0x8B5Cu, 16u}, {0x8B65u, 6u},  {0x8B66u, 8u},  {0x8B67u, 6u},  {0x8B68u, 12u}, {0x8B69u, 8u},
      {0x8B6Au, 12u}, {0x140Au, 1u},  {0x8FFCu, 2u},  {0x8FFDu, 3u},  {0x8FFEu, 4u},  {0x8F46u, 4u},
      {0x8F47u, 9u},  {0x8F48u, 16u}, {0x8F49u, 6u},  {0x8F4Au, 8u},  {0x8F4Bu, 6u},  {0x8F4Cu, 12u},
      {0x8F4Du, 8u},  {0x8F4Eu, 12u},
      /* samplers and images, first and last of each range */
      {0x8B5Du, 1u},  {0x8B64u, 1u},  {0x8DC0u, 1u},  {0x8DC5u, 1u},  {0x8DC9u, 1u},  {0x8DD8u, 1u},
      {0x900Cu, 1u},  {0x900Fu, 1u},  {0x904Cu, 1u},  {0x906Cu, 1u},  {0x9108u, 1u},  {0x910Du, 1u},
      {0x9055u, 1u},
      /* next to a range, and nothing */
      {0x8B4Fu, 0u}, {0x8B6Bu, 0u}, {0x8DD9u, 0u}, {0x9010u, 0u}, {0x906Du, 0u}, {0x910Eu, 0u},
      {0x8DBFu, 0u}, {0x0u, 0u}, {0xFFFFFFFFu, 0u},
  };
  for (const Row &row : rows) {
      const UniformShape shape(7u, 3, row.type);
      CHECK_EQ(shape.values(), row.values);
      CHECK_EQ(shape.program(), 7u);
      CHECK_EQ(shape.location(), 3);
  }
  const UniformShape none;
  CHECK_EQ(none.values(), 0u);
  CHECK_EQ(none.location(), -1); }

TEST(uniform_shapes, remembers_locations_per_program_until_forgotten)
{ UniformShapes shapes;
  CHECK(shapes.invariants_hold());
  shapes.remember(1u, 0, 0x8B5Cu);
  shapes.remember(1u, 4, 0x8B51u);
  shapes.remember(2u, 0, 0x1406u);
  CHECK(shapes.invariants_hold());
  CHECK_EQ(shapes.held(), 3u);
  CHECK_EQ(shapes.values(1u, 0), 16u);
  CHECK_EQ(shapes.values(1u, 4), 3u);
  CHECK_EQ(shapes.values(2u, 0), 1u);
  CHECK_EQ(shapes.values(2u, 4), 0u);
  /* A location answered again takes its new type. */
  shapes.remember(1u, 4, 0x8B52u);
  CHECK_EQ(shapes.values(1u, 4), 4u);
  CHECK_EQ(shapes.held(), 3u);
  shapes.forget(1u);
  CHECK(shapes.invariants_hold());
  CHECK_EQ(shapes.held(), 1u);
  CHECK_EQ(shapes.values(1u, 0), 0u);
  CHECK_EQ(shapes.values(2u, 0), 1u);
  shapes.forget(9u);
  CHECK_EQ(shapes.held(), 1u); }

TEST(uniform_shapes, ignores_a_negative_location_an_unknown_type_and_room_past_capacity)
{ UniformShapes shapes;
  shapes.remember(1u, -1, 0x1406u);
  shapes.remember(1u, 2, 0x1234u);
  CHECK_EQ(shapes.held(), 0u);
  for (std::uint32_t location = 0u; location < UniformShapes::capacity; location++)
      shapes.remember(1u, static_cast<std::int32_t>(location), 0x1406u);
  CHECK_EQ(shapes.held(), UniformShapes::capacity);
  shapes.remember(2u, 0, 0x1406u);
  CHECK_EQ(shapes.held(), UniformShapes::capacity);
  CHECK_EQ(shapes.values(2u, 0), 0u);
  /* A location held can still change type when full. */
  shapes.remember(1u, 5, 0x8B5Bu);
  CHECK_EQ(shapes.values(1u, 5), 9u);
  CHECK(shapes.invariants_hold()); }

TEST(uniform_shapes_invariants, fail_for_rows_no_remember_writes)
{ UniformShapes over;
  av::glicd::UniformShapesProbe::overfill(over);
  CHECK(!over.invariants_hold());
  UniformShapes negative;
  av::glicd::UniformShapesProbe::negative(negative);
  CHECK(!negative.invariants_hold());
  UniformShapes unshaped;
  av::glicd::UniformShapesProbe::unshaped(unshaped);
  CHECK(!unshaped.invariants_hold());
  UniformShapes repeated;
  av::glicd::UniformShapesProbe::repeat(repeated);
  CHECK(!repeated.invariants_hold()); }
