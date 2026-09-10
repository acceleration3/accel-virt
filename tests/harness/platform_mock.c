/* The adversarial platform. Not a stub: it fails allocations on demand, tracks
 * leaks, and enforces the IRQL annotations that src/core declares. */
#include "platform_mock.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_MAX_LIVE 4096

static void      *g_live[MOCK_MAX_LIVE];
static uint32_t   g_live_count;
static uint32_t   g_alloc_seq;
static uint32_t   g_fail_at;
static av_irql_t  g_irql;
static bool       g_irql_violated;
static bool       g_verbose;

void av_mock_reset(void)
{
    for (uint32_t i = 0; i < g_live_count; i++) free(g_live[i]);
    g_live_count = 0;
    g_alloc_seq = 0;
    g_fail_at = 0;
    g_irql = AV_IRQL_PASSIVE;
    g_irql_violated = false;
    g_verbose = getenv("AV_TEST_VERBOSE") != NULL;
}

int      av_mock_leaks(void)       { return (int)g_live_count; }
uint32_t av_mock_alloc_count(void) { return g_alloc_seq; }
void     av_mock_fail_alloc_at(uint32_t nth) { g_fail_at = nth; g_alloc_seq = 0; }
void     av_mock_set_irql(av_irql_t irql)    { g_irql = irql; }
bool     av_mock_irql_violated(void)         { return g_irql_violated; }

av_irql_t av_irql_current(void) { return g_irql; }

void av_irql_require(av_irql_t max, const char *fn)
{
    if (g_irql > max) {
        g_irql_violated = true;
        fprintf(stderr, "\n    IRQL violation: %s requires <= %d, called at %d\n",
                fn, (int)max, (int)g_irql);
    }
}

void *av_alloc(size_t size, av_pool_t pool)
{
    (void)pool;
    /* Allocating above DISPATCH_LEVEL is a bugcheck on Windows. Make it a test
     * failure here instead. */
    if (g_irql >= AV_IRQL_DISPATCH) {
        g_irql_violated = true;
        fprintf(stderr, "\n    allocation at IRQL %d\n", (int)g_irql);
        return NULL;
    }
    g_alloc_seq++;
    if (g_fail_at != 0 && g_alloc_seq == g_fail_at) return NULL;
    if (g_live_count == MOCK_MAX_LIVE) return NULL;
    if (size == 0) return NULL;

    void *p = malloc(size);
    if (p == NULL) return NULL;
    /* Poison, so a caller that reads uninitialized memory is caught. */
    memset(p, 0xA5, size);
    g_live[g_live_count++] = p;
    return p;
}

void av_free(void *p)
{
    if (p == NULL) return;
    for (uint32_t i = 0; i < g_live_count; i++) {
        if (g_live[i] == p) {
            g_live[i] = g_live[--g_live_count];
            free(p);
            return;
        }
    }
    fprintf(stderr, "\n    av_free of untracked pointer %p\n", p);
    abort();
}

void av_memzero(void *p, size_t n) { memset(p, 0, n); }
void av_memcpy(void *d, const void *s, size_t n) { memcpy(d, s, n); }

void av_log(const char *fmt, ...)
{
    if (!g_verbose) return;
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}

#if defined(AV_CHECKED)
void av_invariant_check(bool ok, const char *inv, const char *fn)
{
    if (!ok) {
        fprintf(stderr, "\n    invariant %s failed in %s\n", inv, fn);
        abort();
    }
}
#endif
