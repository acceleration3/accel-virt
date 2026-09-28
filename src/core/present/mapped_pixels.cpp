#include "core/present/mapped_pixels.hpp"

#include "core/platform/platform.hpp"
#include "core/present/present_surface.hpp"
#include "core/present/surface_view.hpp"
#include "core/trace/function_scope.hpp"
#include "proto/commands/present_region.hpp"

#include <immintrin.h>

namespace av {

namespace {

/* Each pixel's four bytes as they are, or with the first and third traded:
 * a shuffle mask over sixteen bytes, whose first four serve a lone pixel. */
alignas(16) constexpr uint8_t same_order[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
alignas(16) constexpr uint8_t swapped_order[16] = {2, 1, 0, 3, 6, 5, 4, 7, 10, 9, 8, 11, 14, 13, 12, 15};

}  // namespace

MappedPixels::MappedPixels(Platform &platform) : platform_(platform)
{
    AV_SCOPE("MappedPixels::MappedPixels");
}

Status MappedPixels::copy(const PresentSurface &source, const PresentSurface &destination,
                          const PresentRegion *regions, uint32_t count)
{
    AV_SCOPE("MappedPixels::copy");
    platform_.require_irql_at_most(Irql::passive, "MappedPixels::copy");
    SurfaceView written(platform_, destination);
    SurfaceView read(platform_, source);
    if (written.map() != Status::ok || read.map() != Status::ok) return Status::not_found;
    const ByteOrder order(source, destination);
    for (uint32_t index = 0u; index < count; index++) {
        const PresentRegion &region = regions[index];
        for (uint32_t row = 0u; row < region.height(); row++)
            order.copy_pixels(read.pixel(region.source_x(), region.source_y() + row),
                              written.pixel(region.destination_x(), region.destination_y() + row), region.width());
    }
    return Status::ok;
}

Status MappedPixels::fill(const PresentSurface &destination, uint32_t colour, const PresentRegion *regions,
                          uint32_t count)
{
    AV_SCOPE("MappedPixels::fill");
    platform_.require_irql_at_most(Irql::passive, "MappedPixels::fill");
    SurfaceView written(platform_, destination);
    if (written.map() != Status::ok) return Status::not_found;
    for (uint32_t index = 0u; index < count; index++) {
        const PresentRegion &region = regions[index];
        for (uint32_t row = 0u; row < region.height(); row++) {
            uint8_t *const to = written.pixel(region.destination_x(), region.destination_y() + row);
            for (uint32_t at = 0u; at < region.width(); at++)
                platform_.copy(to + at * pixel_bytes, &colour, pixel_bytes);
        }
    }
    return Status::ok;
}

MappedPixels::ByteOrder::ByteOrder(const PresentSurface &source, const PresentSurface &destination)
    : order_(source.swaps_with(destination) ? swapped_order : same_order)
{
    AV_SCOPE("MappedPixels::ByteOrder::ByteOrder");
}

/* An ordinary load from an uncached view is a trip to memory: a pixel read a
 * byte at a time was most of a second for a 1280x800 frame, and even memcpy's
 * sixteen-byte loads took fifty milliseconds. MOVNTDQA streams a whole line
 * from write-combined memory per four loads. Windows 11 requires SSE4.2, so
 * every processor it runs on has it. */
__attribute__((target("sse4.1,ssse3"))) void MappedPixels::ByteOrder::copy_pixels(const uint8_t *from, uint8_t *to,
                                                                                  uint32_t count) const
{
    AV_SCOPE("MappedPixels::ByteOrder::copy_pixels");
    constexpr uint32_t line_pixels = 16u / pixel_bytes;
    uint32_t           at = 0u;
    while (at < count && (reinterpret_cast<uintptr_t>(from + at * pixel_bytes) & 15u) != 0u) {
        copy_pixel(from + at * pixel_bytes, to + at * pixel_bytes);
        at++;
    }
    const __m128i shuffle = _mm_load_si128(reinterpret_cast<const __m128i *>(order_));
    for (; at + line_pixels <= count; at += line_pixels) {
        const __m128i pixels = _mm_stream_load_si128(from + at * pixel_bytes);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(to + at * pixel_bytes), _mm_shuffle_epi8(pixels, shuffle));
    }
    for (; at < count; at++) copy_pixel(from + at * pixel_bytes, to + at * pixel_bytes);
}

void MappedPixels::ByteOrder::copy_pixel(const uint8_t *from, uint8_t *to) const
{
    AV_SCOPE("MappedPixels::ByteOrder::copy_pixel");
    uint8_t pixel[pixel_bytes] = {};
    __builtin_memcpy(pixel, from, sizeof pixel);
    const uint8_t moved[pixel_bytes] = {pixel[order_[0]], pixel[order_[1]], pixel[order_[2]], pixel[order_[3]]};
    __builtin_memcpy(to, moved, sizeof moved);
}

}  // namespace av
