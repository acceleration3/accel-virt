#include "av_hostile.h"
#include <stdlib.h>

#define GUARD 64
#define GUARD_BYTE 0x5Au

void av_guarded_alloc(av_guarded_t *g, size_t size)
{
    g->raw = malloc(size + 2 * GUARD);
    memset(g->raw, GUARD_BYTE, size + 2 * GUARD);
    g->base = g->raw + GUARD;
    g->size = size;
    memset(g->base, 0, size);
}

void av_guarded_check(const av_guarded_t *g)
{
    for (size_t i = 0; i < GUARD; i++) {
        if (g->raw[i] != GUARD_BYTE)
            av_test_fail(__FILE__, __LINE__, "underrun at -%zu", GUARD - i);
        if (g->raw[GUARD + g->size + i] != GUARD_BYTE)
            av_test_fail(__FILE__, __LINE__, "overrun at +%zu", i);
    }
}

void av_guarded_free(av_guarded_t *g) { free(g->raw); g->raw = NULL; }

void av_hostile_sweep(av_hostile_parse_fn parse, const uint8_t *valid,
                      size_t valid_len)
{
    uint8_t *buf = malloc(valid_len ? valid_len : 1);

    /* empty and every truncation */
    for (size_t n = 0; n < valid_len; n++) {
        memcpy(buf, valid, n);
        (void)parse(buf, n);
    }
    /* length larger than the data actually present: the parser must trust the
     * length it is given only up to the bound its caller owns */
    memcpy(buf, valid, valid_len);
    (void)parse(buf, valid_len);

    /* byte-flip sweep over a valid input */
    for (size_t i = 0; i < valid_len; i++) {
        for (int bit = 0; bit < 8; bit++) {
            memcpy(buf, valid, valid_len);
            buf[i] ^= (uint8_t)(1u << bit);
            (void)parse(buf, valid_len);
        }
    }
    /* scalar extremes: all zeroes and all ones */
    memset(buf, 0x00, valid_len); (void)parse(buf, valid_len);
    memset(buf, 0xFF, valid_len); (void)parse(buf, valid_len);

    free(buf);
}
