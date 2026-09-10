/* libFuzzer target: drive the GPU VA space with an attacker-chosen sequence of
 * operations and addresses. The contract is that no input causes a crash, a
 * leak, an out-of-bounds translation, or a broken invariant. */
#include <stdint.h>
#include <stddef.h>
#include "../../src/core/gpuva.h"
#include "../harness/platform_mock.h"

#define PAGE AV_GPUVA_PAGE_SIZE

static uint64_t take64(const uint8_t **p, const uint8_t *end)
{
    uint64_t v = 0;
    for (int i = 0; i < 8 && *p < end; i++, (*p)++) v |= (uint64_t)**p << (8 * i);
    return v;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    av_mock_reset();
    av_gpuva_space_t s;
    if (av_gpuva_init(&s, 0x1000000000ull, 0x400000ull) != AV_OK) return 0;
    s.max_ranges = 64;

    const uint8_t *p = data, *end = data + size;
    while (p < end) {
        uint8_t op = *p++;
        switch (op & 0x7u) {
        case 0: { uint64_t sz = take64(&p, end), al = take64(&p, end), b;
                  (void)av_gpuva_reserve(&s, sz, al, &b); break; }
        case 1: { uint64_t b = take64(&p, end), sz = take64(&p, end),
                           off = take64(&p, end);
                  (void)av_gpuva_map(&s, b, sz, (uint32_t)(op >> 3) + 1u, off); break; }
        case 2: { uint64_t b = take64(&p, end);
                  (void)av_gpuva_unmap(&s, b); break; }
        case 3: { uint64_t b = take64(&p, end);
                  (void)av_gpuva_free(&s, b); break; }
        case 4: { uint64_t va = take64(&p, end); uint32_t o; uint64_t f;
                  if (av_gpuva_translate(&s, va, &o, &f) == AV_OK && o == 0u)
                      __builtin_trap();      /* mapped implies non-zero object */
                  break; }
        default: { uint64_t b; (void)av_gpuva_reserve(&s, PAGE, PAGE, &b); break; }
        }
        if (!av_gpuva_invariants(&s)) __builtin_trap();
    }
    av_gpuva_destroy(&s);
    if (av_mock_leaks() != 0) __builtin_trap();
    return 0;
}
