/* Shared hostile-input machinery, so "feed it garbage" is one line per
 * function rather than boilerplate per test.
 *
 * Every case asserts the same contract: an error is returned, nothing outside
 * the destination is written, the call terminates, and ASan/UBSan stay quiet.
 * Never an assert — a malformed input is a reachable state, not a bug. */
#ifndef AV_HOSTILE_H
#define AV_HOSTILE_H

#include <stdint.h>
#include <string.h>
#include "av_test.h"

/* A parser under test: consume `len` bytes at `p`, return non-zero on reject. */
typedef int (*av_hostile_parse_fn)(const uint8_t *p, size_t len);

/* Run the standard sweep against a valid input: empty, every truncation,
 * oversized length, misalignment, scalar extremes and a byte-flip sweep.
 * The parser must never crash and must never report success for a case the
 * caller marked as invalid. */
void av_hostile_sweep(av_hostile_parse_fn parse, const uint8_t *valid,
                      size_t valid_len);

/* A guard region around a destination buffer, to prove nothing wrote past it. */
typedef struct {
    uint8_t *base;      /* start of the usable region  */
    size_t   size;
    uint8_t *raw;
} av_guarded_t;

void av_guarded_alloc(av_guarded_t *g, size_t size);
void av_guarded_check(const av_guarded_t *g);   /* fails the test if smashed */
void av_guarded_free(av_guarded_t *g);

#endif
