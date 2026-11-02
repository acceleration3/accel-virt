#include "icd/d3d/guest_surface_scan.hpp"

#include "core/trace/function_scope.hpp"

#include <immintrin.h>

namespace av::d3d {

namespace {

constexpr uintptr_t vector_mask = 15u;
constexpr size_t    vector_bytes = 16u;
/* MOVNTDQA fills a streaming buffer a line (64 bytes) at a time, and the
 * four loads of a line are what makes it fast. */
constexpr size_t    line_bytes = 64u;

}  // namespace

GuestSurfaceScan::GuestSurfaceScan(uint8_t *shadow, size_t pitch, uint32_t height)
    : shadow_(shadow), pitch_(pitch), height_(height)
{
    AV_SCOPE("GuestSurfaceScan::GuestSurfaceScan");
}

/* The bytes before the first sixteen-byte boundary and after the last whole
 * line or vector are compared as they are: MOVNTDQA wants an aligned address,
 * and a pitch need not be a multiple of sixteen. The shadow is read with
 * unaligned loads. Windows on these guests requires SSE4.1 (Windows 11
 * requires SSE4.2), so there is no runtime fallback; the target attribute keeps
 * the rest of the build's flags as they are. */
__attribute__((target("sse4.1"))) bool GuestSurfaceScan::row_differs(const uint8_t *pages, uint32_t row) const
{
    AV_SCOPE("GuestSurfaceScan::row_differs");
    const uint8_t *page = pages + row * pitch_;
    const uint8_t *shadow = shadow_ + row * pitch_;
    size_t         left = pitch_;
    size_t         head = (vector_bytes - (reinterpret_cast<uintptr_t>(page) & vector_mask)) & vector_mask;
    if (head > left) head = left;
    if (head != 0u && __builtin_memcmp(page, shadow, head) != 0) return true;
    page += head;
    shadow += head;
    left -= head;
    for (; left >= line_bytes; left -= line_bytes, page += line_bytes, shadow += line_bytes) {
        const __m128i p0 = _mm_stream_load_si128(page);
        const __m128i p1 = _mm_stream_load_si128(page + vector_bytes);
        const __m128i p2 = _mm_stream_load_si128(page + 2u * vector_bytes);
        const __m128i p3 = _mm_stream_load_si128(page + 3u * vector_bytes);
        const __m128i d0 = _mm_xor_si128(p0, _mm_loadu_si128(reinterpret_cast<const __m128i *>(shadow)));
        const __m128i d1 = _mm_xor_si128(p1, _mm_loadu_si128(reinterpret_cast<const __m128i *>(shadow + vector_bytes)));
        const __m128i d2 = _mm_xor_si128(p2, _mm_loadu_si128(reinterpret_cast<const __m128i *>(shadow + 2u * vector_bytes)));
        const __m128i d3 = _mm_xor_si128(p3, _mm_loadu_si128(reinterpret_cast<const __m128i *>(shadow + 3u * vector_bytes)));
        const __m128i any = _mm_or_si128(_mm_or_si128(d0, d1), _mm_or_si128(d2, d3));
        if (_mm_testz_si128(any, any) == 0) return true;
    }
    for (; left >= vector_bytes; left -= vector_bytes, page += vector_bytes, shadow += vector_bytes) {
        const __m128i diff = _mm_xor_si128(_mm_stream_load_si128(page), _mm_loadu_si128(reinterpret_cast<const __m128i *>(shadow)));
        if (_mm_testz_si128(diff, diff) == 0) return true;
    }
    return left != 0u && __builtin_memcmp(page, shadow, left) != 0;
}

/* The band is one run of bytes from the first row's start, so only its start
 * can be unaligned. */
__attribute__((target("sse4.1"))) void GuestSurfaceScan::copy_rows(const uint8_t *pages, uint32_t first, uint32_t end)
{
    AV_SCOPE("GuestSurfaceScan::copy_rows");
    const uint8_t *from = pages + first * pitch_;
    uint8_t       *to = shadow_ + first * pitch_;
    size_t         left = static_cast<size_t>(end - first) * pitch_;
    size_t         head = (vector_bytes - (reinterpret_cast<uintptr_t>(from) & vector_mask)) & vector_mask;
    if (head > left) head = left;
    __builtin_memcpy(to, from, head);
    from += head;
    to += head;
    left -= head;
    for (; left >= vector_bytes; left -= vector_bytes, from += vector_bytes, to += vector_bytes)
        _mm_storeu_si128(reinterpret_cast<__m128i *>(to), _mm_stream_load_si128(from));
    __builtin_memcpy(to, from, left);
}

RowBand GuestSurfaceScan::differing(const uint8_t *pages)
{
    AV_SCOPE("GuestSurfaceScan::differing");
    uint32_t first = height_, last = 0u;
    for (uint32_t row = 0u; row < height_; row++) {
        if (!row_differs(pages, row)) continue;
        if (first == height_) first = row;
        last = row;
    }
    if (first == height_) return RowBand();
    copy_rows(pages, first, last + 1u);
    return RowBand(first, last + 1u);
}

void GuestSurfaceScan::copy_all(const uint8_t *pages)
{
    AV_SCOPE("GuestSurfaceScan::copy_all");
    copy_rows(pages, 0u, height_);
}

}  // namespace av::d3d
