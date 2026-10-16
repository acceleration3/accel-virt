#include "guest/probe/coverage_triangle.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

namespace {

/* A sample position in sixteenths of a pixel from the pixel's center. */
struct SamplePosition {
    int8_t x;
    int8_t y;
};

/* D3D's standard patterns for 2, 4, 8 and 16 samples, one after another. */
constexpr SamplePosition positions[30] = {
    {4, 4},  {-4, -4},                                                               // 2
    {-2, -6}, {6, -2}, {-6, 2}, {2, 6},                                              // 4
    {1, -3}, {-1, 3}, {5, 1}, {-3, -5}, {-5, 5}, {-7, -1}, {3, 7}, {7, -7},          // 8
    {1, 1},   {-1, -3}, {-3, 2}, {4, -1}, {-5, -2}, {2, 5},  {5, 3}, {3, -5},        // 16
    {-2, 6},  {0, -7},  {-4, -6}, {-6, 4}, {-8, 0}, {7, -4}, {6, 7}, {-7, -8}};

/* Subpixels per sixteenth of a pixel. */
constexpr int64_t sixteenth = CoverageTriangle::subpixels / 16;

}  // namespace

CoverageTriangle::CoverageTriangle(const int32_t (&x)[3], const int32_t (&y)[3]) : x_{}, y_{}, area_(false)
{
    AV_SCOPE("CoverageTriangle::CoverageTriangle");
    for (uint32_t vertex = 0u; vertex < 3u; vertex++) {
        x_[vertex] = x[vertex];
        y_[vertex] = y[vertex];
    }
    const int64_t area = (x_[1] - x_[0]) * (y_[2] - y_[0]) - (y_[1] - y_[0]) * (x_[2] - x_[0]);
    area_ = area != 0;
    /* Wound so that the interior has positive edge functions. */
    if (area < 0) {
        const int64_t x1 = x_[1], y1 = y_[1];
        x_[1] = x_[2];
        y_[1] = y_[2];
        x_[2] = x1;
        y_[2] = y1;
    }
}

uint32_t CoverageTriangle::coverage(uint32_t samples, uint32_t px, uint32_t py) const
{
    AV_SCOPE("CoverageTriangle::coverage");
    uint32_t first = 0u;
    switch (samples) {
    case 2u: first = 0u; break;
    case 4u: first = 2u; break;
    case 8u: first = 6u; break;
    case 16u: first = 14u; break;
    default: return 0u;
    }
    const int64_t center_x = static_cast<int64_t>(px) * subpixels + subpixels / 2;
    const int64_t center_y = static_cast<int64_t>(py) * subpixels + subpixels / 2;
    uint32_t mask = 0u;
    for (uint32_t sample = 0u; sample < samples; sample++) {
        const SamplePosition &at = positions[first + sample];
        if (covers(center_x + at.x * sixteenth, center_y + at.y * sixteenth)) mask |= 1u << sample;
    }
    return mask;
}

bool CoverageTriangle::covers(int64_t x, int64_t y) const
{
    AV_SCOPE("CoverageTriangle::covers");
    if (!area_) return false;
    for (uint32_t edge = 0u; edge < 3u; edge++) {
        const uint32_t next = (edge + 1u) % 3u;
        const int64_t  dx = x_[next] - x_[edge];
        const int64_t  dy = y_[next] - y_[edge];
        const int64_t  value = dx * (y - y_[edge]) - dy * (x - x_[edge]);
        const bool     top_left = dy < 0 || (dy == 0 && dx > 0);
        if (value < 0 || (value == 0 && !top_left)) return false;
    }
    return true;
}

}  // namespace av
