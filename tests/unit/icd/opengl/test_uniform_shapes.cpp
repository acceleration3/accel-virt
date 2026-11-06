#include "core/support/status.hpp"
#include "harness/framework/test_registry.hpp"
#include "harness/platform/mock_platform.hpp"
#include "icd/opengl/uniform_shape.hpp"
#include "icd/opengl/uniform_shapes.hpp"

#include <cstdint>

using av::MockPlatform;
using av::Status;
using av::glicd::UniformShape;
using av::glicd::UniformShapes;

namespace av {

struct KeyedSlotsProbe {
    template <typename Key, typename Entry>
    static void corrupt_count(KeyedSlots<Key, Entry> &table, std::uint32_t count) { table.count_ = count; }
};

}

namespace av::glicd {

struct UniformShapesProbe {
    static void corrupt_table(UniformShapes &shapes) { KeyedSlotsProbe::corrupt_count(shapes.values_, 99u); }
    static void plant(UniformShapes &shapes, std::uint64_t key, std::uint32_t values)
    {
        std::uint32_t slot = 0u;
        static_cast<void>(shapes.values_.insert(key, values, slot));
    }
};

}

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
      {0x8B5Du, 1u},  {0x8B64u, 1u},  {0x8DC0u, 1u},  {0x8DC5u, 1u},  {0x8DC9u, 1u},  {0x8DD8u, 1u},
      {0x900Cu, 1u},  {0x900Fu, 1u},  {0x904Cu, 1u},  {0x906Cu, 1u},  {0x9108u, 1u},  {0x910Du, 1u},
      {0x9055u, 1u},
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
{ MockPlatform  platform;
  UniformShapes shapes(platform);
  CHECK(shapes.invariants_hold());
  CHECK_STATUS(shapes.remember(1u, 0, 0x8B5Cu), Status::ok);
  CHECK_STATUS(shapes.remember(1u, 4, 0x8B51u), Status::ok);
  CHECK_STATUS(shapes.remember(2u, 0, 0x1406u), Status::ok);
  CHECK(shapes.invariants_hold());
  CHECK_EQ(shapes.held(), 3u);
  CHECK_EQ(shapes.values(1u, 0), 16u);
  CHECK_EQ(shapes.values(1u, 4), 3u);
  CHECK_EQ(shapes.values(2u, 0), 1u);
  CHECK_EQ(shapes.values(2u, 4), 0u);
  CHECK_STATUS(shapes.remember(1u, 4, 0x8B52u), Status::ok);
  CHECK_EQ(shapes.values(1u, 4), 4u);
  CHECK_EQ(shapes.held(), 3u);
  shapes.forget(1u);
  CHECK(shapes.invariants_hold());
  CHECK_EQ(shapes.held(), 1u);
  CHECK_EQ(shapes.values(1u, 0), 0u);
  CHECK_EQ(shapes.values(2u, 0), 1u);
  shapes.forget(9u);
  CHECK_EQ(shapes.held(), 1u); }

TEST(uniform_shapes, ignores_a_negative_location_and_an_unknown_type)
{ MockPlatform  platform;
  UniformShapes shapes(platform);
  CHECK_STATUS(shapes.remember(1u, -1, 0x1406u), Status::ok);
  CHECK_STATUS(shapes.remember(1u, 2, 0x1234u), Status::ok);
  CHECK_EQ(shapes.held(), 0u);
  CHECK_EQ(shapes.values(1u, -1), 0u);
  CHECK(shapes.invariants_hold()); }

TEST(uniform_shapes, keeps_every_location_past_the_old_bound)
{ MockPlatform  platform;
  UniformShapes shapes(platform);
  for (std::uint32_t program = 1u; program <= 20u; program++)
      for (std::int32_t location = 0; location < 500; location++)
          CHECK_STATUS(shapes.remember(program, location, program % 2u == 0u ? 0x8B5Cu : 0x1406u), Status::ok);
  CHECK_EQ(shapes.held(), 10000u);
  CHECK(shapes.invariants_hold());
  CHECK_EQ(shapes.values(20u, 499), 16u);
  CHECK_EQ(shapes.values(19u, 0), 1u);
  shapes.forget(20u);
  CHECK_EQ(shapes.held(), 9500u);
  CHECK_EQ(shapes.values(20u, 499), 0u);
  CHECK_EQ(shapes.values(18u, 499), 16u);
  CHECK(shapes.invariants_hold()); }

TEST(uniform_shapes, a_location_with_no_memory_to_keep_it_is_refused)
{ MockPlatform  platform;
  UniformShapes shapes(platform);
  platform.fail_allocation_at(1u);
  CHECK_STATUS(shapes.remember(1u, 0, 0x1406u), Status::no_memory);
  CHECK_EQ(shapes.values(1u, 0), 0u);
  CHECK_EQ(shapes.held(), 0u);
  CHECK(shapes.invariants_hold()); }

TEST(uniform_shapes_invariants, fail_for_rows_no_remember_writes)
{ MockPlatform  platform;
  UniformShapes negative(platform);
  av::glicd::UniformShapesProbe::plant(negative, std::uint64_t{1u} << 32u | 0xFFFFFFFFu, 1u);
  CHECK(!negative.invariants_hold());
  UniformShapes unshaped(platform);
  av::glicd::UniformShapesProbe::plant(unshaped, std::uint64_t{1u} << 32u, 0u);
  CHECK(!unshaped.invariants_hold());
  UniformShapes both(platform);
  av::glicd::UniformShapesProbe::plant(both, std::uint64_t{1u} << 32u, 0u);
  av::glicd::UniformShapesProbe::plant(both, std::uint64_t{2u} << 32u, 0u);
  CHECK(!both.invariants_hold());
  UniformShapes table(platform);
  CHECK_STATUS(table.remember(1u, 0, 0x1406u), Status::ok);
  CHECK(table.invariants_hold());
  av::glicd::UniformShapesProbe::corrupt_table(table);
  CHECK(!table.invariants_hold()); }
