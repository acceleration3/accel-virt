#include "harness/framework/test_registry.hpp"
#include "icd/opengl/query_shapes.hpp"

#include <cstdint>

using av::glicd::QueryShapes;
using List = QueryShapes::List;

namespace av::glicd {

struct QueryShapesProbe {
    static void overflow(QueryShapes &shapes) { shapes.lengths_[1] = 0x80000000u; }
};

}

TEST(query_shapes, list_queries_answer_what_was_learned)
{ QueryShapes shapes;
  CHECK_EQ(shapes.values(0x86A3u), 0u);
  CHECK_EQ(shapes.values(0x87FFu), 0u);
  CHECK_EQ(shapes.values(0x8DF8u), 0u);
  shapes.learn(List::compressed_texture_formats, 12);
  shapes.learn(List::program_binary_formats, 1);
  shapes.learn(List::shader_binary_formats, -3);
  CHECK(shapes.invariants_hold());
  CHECK_EQ(shapes.values(0x86A3u), 12u);
  CHECK_EQ(shapes.values(0x87FFu), 1u);
  CHECK_EQ(shapes.values(0x8DF8u), 0u);
  CHECK_EQ(QueryShapes::length_queries[0], 0x86A2u);
  CHECK_EQ(QueryShapes::length_queries[1], 0x87FEu);
  CHECK_EQ(QueryShapes::length_queries[2], 0x8DF9u); }

TEST(query_shapes, other_queries_answer_the_registry_count)
{ const QueryShapes shapes;
  CHECK_EQ(shapes.values(0x0BA2u ), 4u);
  CHECK_EQ(shapes.values(0x0BA6u ), 16u);
  CHECK_EQ(shapes.values(0x0B72u ), 1u);
  CHECK_EQ(shapes.values(0x86A2u), 1u); }

TEST(query_shapes, a_uniform_blocks_index_list_is_not_sized)
{ const QueryShapes shapes;
  CHECK_EQ(shapes.block_values(0x8A43u), 0u);
  CHECK_EQ(shapes.block_values(0x8A42u), 1u);
  CHECK_EQ(shapes.block_values(0x8A40u), 1u); }

TEST(query_shapes_invariants, fail_for_a_length_no_glint_says)
{ QueryShapes shapes;
  av::glicd::QueryShapesProbe::overflow(shapes);
  CHECK(!shapes.invariants_hold()); }
