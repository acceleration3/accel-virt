/* GPU virtual address space.
 *
 * The driver reports GPU MMU support to dxgkrnl, so VidMm hands out GPU
 * virtual addresses and asks the driver to maintain the translation. There is
 * no real page table: a VA range translates to (device object, offset), which
 * the QEMU device resolves to host memory. See
 * ../claude-scratch/wddm-kb/kb/04-memory.md. */
#ifndef AV_GPUVA_H
#define AV_GPUVA_H

#include "platform.h"

#define AV_GPUVA_PAGE_SIZE  4096u
#define AV_GPUVA_MAX_RANGES 65536u

typedef struct {
    uint64_t base;           /* GPU VA, page aligned                       */
    uint64_t size;           /* bytes, non-zero, page multiple             */
    uint32_t object;         /* device object id; 0 when reserved-not-mapped */
    uint64_t object_offset;  /* byte offset within that object             */
    bool     mapped;
} av_gpuva_range_t;

typedef struct {
    av_gpuva_range_t *ranges;    /* sorted by base, never overlapping */
    uint32_t          count;
    uint32_t          capacity;
    uint64_t          va_base;
    uint64_t          va_size;
    /* Table ceiling. init() sets AV_GPUVA_MAX_RANGES; tests lower it so the
     * exhaustion branch is reachable without allocating 64K ranges. */
    uint32_t          max_ranges;
} av_gpuva_space_t;

av_status_t av_gpuva_init(av_gpuva_space_t *s, uint64_t va_base,
                          uint64_t va_size) AV_AT_PASSIVE;
void        av_gpuva_destroy(av_gpuva_space_t *s) AV_AT_PASSIVE;

/* Find a free aligned hole of `size` and record it as reserved. */
av_status_t av_gpuva_reserve(av_gpuva_space_t *s, uint64_t size,
                             uint64_t align, uint64_t *out_base) AV_AT_PASSIVE;

/* Bind a previously reserved range to a device object. */
av_status_t av_gpuva_map(av_gpuva_space_t *s, uint64_t base, uint64_t size,
                         uint32_t object, uint64_t object_offset) AV_AT_PASSIVE;

av_status_t av_gpuva_unmap(av_gpuva_space_t *s, uint64_t base) AV_AT_PASSIVE;
av_status_t av_gpuva_free(av_gpuva_space_t *s, uint64_t base) AV_AT_PASSIVE;

/* Translate. Called on the submission path, so it must not allocate. */
av_status_t av_gpuva_translate(const av_gpuva_space_t *s, uint64_t va,
                               uint32_t *object,
                               uint64_t *object_offset) AV_AT_DISPATCH;

bool av_gpuva_invariants(const av_gpuva_space_t *s);

/* Round `v` up to a multiple of `align` (a power of two), reporting overflow
 * rather than wrapping. Exposed so its overflow branch is directly testable
 * instead of only reachable through a pathological address space. */
bool av_gpuva_align_up(uint64_t v, uint64_t align, uint64_t *out);

#endif /* AV_GPUVA_H */
