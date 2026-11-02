#include "guest/probe/coverage_triangle.hpp"
#include "harness/framework/test_registry.hpp"

#include <cstdint>

using av::CoverageTriangle;

namespace {
constexpr std::int32_t center = 128;
constexpr std::int32_t sample0 = center + 16;

std::uint32_t coverage(std::int32_t x0, std::int32_t y0, std::int32_t x1, std::int32_t y1, std::int32_t x2,
                       std::int32_t y2, std::uint32_t samples = 16u)
{
    const std::int32_t x[3] = {x0, x1, x2};
    const std::int32_t y[3] = {y0, y1, y2};
    return CoverageTriangle(x, y).coverage(samples, 0u, 0u);
}

}

TEST(coverage_triangle, a_pixel_inside_has_every_sample_of_every_pattern)
{
    CHECK_EQ(coverage(-1024, -1024, 4096, -1024, -1024, 4096, 2u), 0x3u);
    CHECK_EQ(coverage(-1024, -1024, 4096, -1024, -1024, 4096, 4u), 0xfu);
    CHECK_EQ(coverage(-1024, -1024, 4096, -1024, -1024, 4096, 8u), 0xffu);
    CHECK_EQ(coverage(-1024, -1024, 4096, -1024, -1024, 4096, 16u), 0xffffu);
    const std::int32_t x[3] = {-1024, 4096, -1024};
    const std::int32_t y[3] = {-1024, -1024, 4096};
    CHECK_EQ(CoverageTriangle(x, y).coverage(16u, 3u, 3u), 0xffffu);
    CHECK_EQ(CoverageTriangle(x, y).coverage(16u, 14u, 14u), 0u);
}

TEST(coverage_triangle, a_count_naming_no_standard_pattern_covers_nothing)
{
    const std::uint32_t counts[] = {0u, 1u, 3u, 32u};
    for (const std::uint32_t samples : counts)
        CHECK_EQ(coverage(-1024, -1024, 4096, -1024, -1024, 4096, samples), 0u);
}

TEST(coverage_triangle, a_sample_on_a_vertical_edge_belongs_to_the_triangle_on_its_right)
{
    const std::uint32_t right = coverage(sample0, -512, 1024, -512, sample0, 1024);
    const std::uint32_t left = coverage(sample0, -512, -1024, -512, sample0, 1024);
    CHECK_EQ(right & 1u, 1u);
    CHECK_EQ(left & 1u, 0u);
    CHECK_EQ(right & left, 0u);
    CHECK_EQ(right | left, 0xffffu);
}

TEST(coverage_triangle, a_sample_on_a_horizontal_edge_belongs_to_the_triangle_below)
{
    const std::uint32_t below = coverage(-512, sample0, 1024, sample0, -512, 1024);
    const std::uint32_t above = coverage(-512, sample0, 1024, sample0, -512, -1024);
    CHECK_EQ(below & 1u, 1u);
    CHECK_EQ(above & 1u, 0u);
    CHECK_EQ(below & above, 0u);
    CHECK_EQ(below | above, 0xffffu);
}

TEST(coverage_triangle, a_sample_on_a_shared_diagonal_is_covered_once)
{
    const std::uint32_t upper = coverage(0, 0, 288, 0, 288, 288);
    const std::uint32_t lower = coverage(0, 0, 288, 288, 0, 288);
    CHECK_EQ(upper & lower, 0u);
    CHECK_EQ(upper & 1u, 1u);
    CHECK_EQ(lower & 1u, 0u);
}

TEST(coverage_triangle, the_winding_does_not_change_the_coverage)
{
    const std::uint32_t clockwise = coverage(40, 30, 250, 90, 100, 240);
    const std::uint32_t counter = coverage(40, 30, 100, 240, 250, 90);
    CHECK(clockwise != 0u && clockwise != 0xffffu);
    CHECK_EQ(clockwise, counter);
}

TEST(coverage_triangle, a_triangle_with_no_area_covers_nothing)
{
    CHECK_EQ(coverage(0, 0, 256, 256, 512, 512), 0u);
    CHECK_EQ(coverage(sample0, sample0, sample0, sample0, sample0, sample0), 0u);
}

TEST(coverage_triangle, far_vertices_are_exact)
{
    constexpr std::int32_t far = 1 << 29;
    CHECK_EQ(coverage(-far, -far, far, -far, 0, far), 0xffffu);
    CHECK_EQ(coverage(sample0, -far, far, far, sample0, far) & 1u, 1u);
    CHECK_EQ(coverage(sample0 + 1, -far, far, far, sample0 + 1, far) & 1u, 0u);
}
