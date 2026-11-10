#include "executor/d3d/d3d9_line_ends.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

namespace {

constexpr std::uint32_t point_list = 1u;
constexpr std::uint32_t line_list = 2u;
constexpr std::uint32_t line_strip = 3u;
constexpr std::uint32_t triangle_list = 4u;
constexpr std::uint32_t last_pixel_state = 16u;
constexpr std::uint32_t cull_state = 22u;

}

TEST(d3d9_line_ends, only_lines_get_an_end_point_and_the_default_is_on)
{
    av::D3d9LineEnds ends;
    CHECK(ends.wanted(line_list));
    CHECK(ends.wanted(line_strip));
    CHECK(!ends.wanted(point_list));
    CHECK(!ends.wanted(triangle_list));
}

TEST(d3d9_line_ends, the_last_pixel_state_turns_them_off_and_on_and_other_states_change_nothing)
{
    av::D3d9LineEnds ends;
    ends.render_state(last_pixel_state, 0u);
    CHECK(!ends.wanted(line_list));
    ends.render_state(cull_state, 1u);
    CHECK(!ends.wanted(line_strip));
    ends.render_state(last_pixel_state, 1u);
    CHECK(ends.wanted(line_list));
    ends.render_state(cull_state, 0u);
    CHECK(ends.wanted(line_strip));
}
