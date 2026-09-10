#include "gpuva.h"

#define PAGE_MASK (uint64_t)(AV_GPUVA_PAGE_SIZE - 1u)

static bool is_page_aligned(uint64_t v) { return (v & PAGE_MASK) == 0u; }

static bool add_overflows(uint64_t a, uint64_t b)
{
    return a > UINT64_MAX - b;
}

static bool is_pow2(uint64_t v) { return v != 0u && (v & (v - 1u)) == 0u; }

bool av_gpuva_align_up(uint64_t v, uint64_t align, uint64_t *out)
{
    if (out == NULL || !is_pow2(align)) return false;
    if (add_overflows(v, align - 1u)) return false;
    *out = (v + align - 1u) & ~(align - 1u);
    return true;
}

/* Index of the first range whose base >= `base`. */
static uint32_t lower_bound(const av_gpuva_space_t *s, uint64_t base)
{
    uint32_t lo = 0u, hi = s->count;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2u;
        if (s->ranges[mid].base < base) lo = mid + 1u;
        else                            hi = mid;
    }
    return lo;
}

static av_gpuva_range_t *find_exact(av_gpuva_space_t *s, uint64_t base)
{
    uint32_t i = lower_bound(s, base);
    if (i < s->count && s->ranges[i].base == base) return &s->ranges[i];
    return NULL;
}

static av_status_t grow(av_gpuva_space_t *s)
{
    uint32_t cap = (s->capacity == 0u) ? 16u : s->capacity * 2u;
    if (cap > s->max_ranges) {
        if (s->capacity >= s->max_ranges) return AV_E_NO_SPACE;
        cap = s->max_ranges;
    }
    av_gpuva_range_t *n = av_alloc((size_t)cap * sizeof *n, AV_POOL_NONPAGED);
    if (n == NULL) return AV_E_NO_MEMORY;
    av_memzero(n, (size_t)cap * sizeof *n);
    if (s->count != 0u) av_memcpy(n, s->ranges, (size_t)s->count * sizeof *n);
    av_free(s->ranges);
    s->ranges = n;
    s->capacity = cap;
    return AV_OK;
}

static av_status_t insert_at(av_gpuva_space_t *s, uint32_t idx,
                             const av_gpuva_range_t *r)
{
    if (s->count == s->capacity) {
        av_status_t st = grow(s);
        if (st != AV_OK) return st;
    }
    for (uint32_t i = s->count; i > idx; i--) s->ranges[i] = s->ranges[i - 1u];
    s->ranges[idx] = *r;
    s->count++;
    return AV_OK;
}

av_status_t av_gpuva_init(av_gpuva_space_t *s, uint64_t va_base, uint64_t va_size)
{
    av_irql_require(AV_IRQL_PASSIVE, __func__);
    if (s == NULL)                       return AV_E_INVALID_PARAM;
    if (va_size == 0u)                   return AV_E_INVALID_PARAM;
    if (!is_page_aligned(va_base))       return AV_E_INVALID_PARAM;
    if (!is_page_aligned(va_size))       return AV_E_INVALID_PARAM;
    if (add_overflows(va_base, va_size)) return AV_E_INVALID_PARAM;

    s->ranges = NULL;
    s->count = 0u;
    s->capacity = 0u;
    s->va_base = va_base;
    s->va_size = va_size;
    s->max_ranges = AV_GPUVA_MAX_RANGES;
    return AV_OK;
}

void av_gpuva_destroy(av_gpuva_space_t *s)
{
    av_irql_require(AV_IRQL_PASSIVE, __func__);
    if (s == NULL) return;
    av_free(s->ranges);
    s->ranges = NULL;
    s->count = 0u;
    s->capacity = 0u;
}

av_status_t av_gpuva_reserve(av_gpuva_space_t *s, uint64_t size, uint64_t align,
                             uint64_t *out_base)
{
    av_irql_require(AV_IRQL_PASSIVE, __func__);
    if (s == NULL || out_base == NULL)   return AV_E_INVALID_PARAM;
    if (size == 0u || !is_page_aligned(size)) return AV_E_INVALID_PARAM;
    if (!is_pow2(align) || align < AV_GPUVA_PAGE_SIZE) return AV_E_INVALID_PARAM;
    if (size > s->va_size)               return AV_E_TOO_LARGE;
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);

    uint64_t limit = s->va_base + s->va_size;
    uint64_t cursor;
    if (!av_gpuva_align_up(s->va_base, align, &cursor)) return AV_E_NO_SPACE;

    for (uint32_t i = 0u; i <= s->count; i++) {
        uint64_t next = (i < s->count) ? s->ranges[i].base : limit;
        if (cursor < next && next - cursor >= size) {
            av_gpuva_range_t r = { cursor, size, 0u, 0u, false };
            av_status_t st = insert_at(s, i, &r);
            if (st != AV_OK) return st;
            *out_base = cursor;
            AV_INVARIANT_SCOPE(av_gpuva_invariants, s);
            return AV_OK;
        }
        if (i < s->count) {
            uint64_t end = s->ranges[i].base + s->ranges[i].size;
            uint64_t aligned;
            if (!av_gpuva_align_up(end, align, &aligned)) return AV_E_NO_SPACE;
            /* An aligned cursor past the end of the space cannot host a hole. */
            if (aligned >= limit) return AV_E_NO_SPACE;
            if (aligned > cursor) cursor = aligned;
        }
    }
    return AV_E_NO_SPACE;
}

av_status_t av_gpuva_map(av_gpuva_space_t *s, uint64_t base, uint64_t size,
                         uint32_t object, uint64_t object_offset)
{
    av_irql_require(AV_IRQL_PASSIVE, __func__);
    if (s == NULL)     return AV_E_INVALID_PARAM;
    if (object == 0u)  return AV_E_INVALID_PARAM;
    if (add_overflows(object_offset, size)) return AV_E_INVALID_PARAM;
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);

    av_gpuva_range_t *r = find_exact(s, base);
    if (r == NULL)      return AV_E_NOT_FOUND;
    if (r->size != size) return AV_E_INVALID_PARAM;
    if (r->mapped)      return AV_E_CONFLICT;

    r->object = object;
    r->object_offset = object_offset;
    r->mapped = true;
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);
    return AV_OK;
}

av_status_t av_gpuva_unmap(av_gpuva_space_t *s, uint64_t base)
{
    av_irql_require(AV_IRQL_PASSIVE, __func__);
    if (s == NULL) return AV_E_INVALID_PARAM;
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);

    av_gpuva_range_t *r = find_exact(s, base);
    if (r == NULL)   return AV_E_NOT_FOUND;
    if (!r->mapped)  return AV_E_INVALID_STATE;

    r->object = 0u;
    r->object_offset = 0u;
    r->mapped = false;
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);
    return AV_OK;
}

av_status_t av_gpuva_free(av_gpuva_space_t *s, uint64_t base)
{
    av_irql_require(AV_IRQL_PASSIVE, __func__);
    if (s == NULL) return AV_E_INVALID_PARAM;
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);

    uint32_t i = lower_bound(s, base);
    if (i >= s->count || s->ranges[i].base != base) return AV_E_NOT_FOUND;
    if (s->ranges[i].mapped) return AV_E_INVALID_STATE;

    for (uint32_t j = i; j + 1u < s->count; j++) s->ranges[j] = s->ranges[j + 1u];
    s->count--;
    av_memzero(&s->ranges[s->count], sizeof s->ranges[s->count]);
    AV_INVARIANT_SCOPE(av_gpuva_invariants, s);
    return AV_OK;
}

av_status_t av_gpuva_translate(const av_gpuva_space_t *s, uint64_t va,
                               uint32_t *object, uint64_t *object_offset)
{
    av_irql_require(AV_IRQL_DISPATCH, __func__);
    if (s == NULL || object == NULL || object_offset == NULL)
        return AV_E_INVALID_PARAM;

    uint32_t i = lower_bound(s, va);
    /* lower_bound lands on the range starting at va, or the one after it. */
    if (i == s->count || s->ranges[i].base != va) {
        if (i == 0u) return AV_E_NOT_FOUND;
        i--;
    }
    /* lower_bound guarantees r->base <= va here: either it matched exactly, or
     * we stepped back to the last range that starts below va. Only the upper
     * bound still has to be checked. */
    const av_gpuva_range_t *r = &s->ranges[i];
    if (va - r->base >= r->size) return AV_E_NOT_FOUND;
    if (!r->mapped) return AV_E_INVALID_STATE;

    *object = r->object;
    *object_offset = r->object_offset + (va - r->base);
    return AV_OK;
}

bool av_gpuva_invariants(const av_gpuva_space_t *s)
{
    if (s == NULL) return false;
    if (s->count > s->capacity) return false;
    if (s->capacity != 0u && s->ranges == NULL) return false;
    if (s->capacity > s->max_ranges) return false;
    if (s->max_ranges == 0u || s->max_ranges > AV_GPUVA_MAX_RANGES) return false;
    if (add_overflows(s->va_base, s->va_size)) return false;

    uint64_t limit = s->va_base + s->va_size;
    uint64_t prev_end = s->va_base;
    for (uint32_t i = 0u; i < s->count; i++) {
        const av_gpuva_range_t *r = &s->ranges[i];
        if (r->size == 0u) return false;
        if (!is_page_aligned(r->base) || !is_page_aligned(r->size)) return false;
        if (add_overflows(r->base, r->size)) return false;
        if (r->base < prev_end) return false;              /* sorted, no overlap */
        if (r->base + r->size > limit) return false;       /* inside the space   */
        if (r->mapped != (r->object != 0u)) return false;  /* mapped iff object  */
        if (!r->mapped && r->object_offset != 0u) return false;
        if (add_overflows(r->object_offset, r->size)) return false;
        prev_end = r->base + r->size;
    }
    return true;
}
