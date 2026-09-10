/* The only interface src/core has to the outside world.
 *
 * Two implementations: src/km/platform_km.c for the Windows kernel, and
 * tests/harness/platform_mock.c for the native test build. Nothing in
 * src/core may include a WDM, dxgk, QEMU or libc header. */
#ifndef AV_PLATFORM_H
#define AV_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* IRQL. Annotations are load-bearing: the mock enforces them, so a violation
 * is a failing unit test rather than a bugcheck in a VM. */
typedef enum {
    AV_IRQL_PASSIVE  = 0,
    AV_IRQL_APC      = 1,
    AV_IRQL_DISPATCH = 2,
    AV_IRQL_ISR      = 3
} av_irql_t;

/* Maximum IRQL at which the function may be called. Declared on every public
 * core function. Checked by av_irql_require() in debug and test builds. */
#define AV_AT_PASSIVE   /* callable only at PASSIVE_LEVEL */
#define AV_AT_DISPATCH  /* callable at <= DISPATCH_LEVEL: no block, no alloc */
#define AV_AT_ISR       /* callable at device IRQL */

av_irql_t av_irql_current(void);
void      av_irql_require(av_irql_t max, const char *fn);

typedef enum { AV_POOL_NONPAGED = 0, AV_POOL_PAGED = 1 } av_pool_t;

void *av_alloc(size_t size, av_pool_t pool);
void  av_free(void *p);
void  av_memzero(void *p, size_t n);
void  av_memcpy(void *dst, const void *src, size_t n);

/* Status codes. Values chosen to map 1:1 onto the NTSTATUS the shim returns,
 * without src/core knowing what an NTSTATUS is. */
typedef enum {
    AV_OK                 = 0,
    AV_E_INVALID_PARAM    = 1,
    AV_E_NO_MEMORY        = 2,
    AV_E_NOT_FOUND        = 3,
    AV_E_CONFLICT         = 4,   /* range overlap, duplicate id */
    AV_E_NO_SPACE         = 5,
    AV_E_NOT_SUPPORTED    = 6,
    AV_E_INVALID_STATE    = 7,
    AV_E_TOO_LARGE        = 8
} av_status_t;

void av_log(const char *fmt, ...);

/* Invariant scoping. In test and checked builds this asserts the module's
 * invariant on entry and exit; in release it compiles away. */
#if defined(AV_CHECKED)
#  define AV_INVARIANT_SCOPE(fn, obj) av_invariant_check((fn)(obj), #fn, __func__)
void av_invariant_check(bool ok, const char *inv, const char *fn);
#else
#  define AV_INVARIANT_SCOPE(fn, obj) ((void)0)
#endif

/* A branch that cannot be reached from a test. Counted by the coverage gate;
 * every use must be justified in review. */
#define AV_UNTESTABLE(reason) /* reason */

#endif /* AV_PLATFORM_H */
