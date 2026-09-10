/* A test framework small enough to have no dependencies and no surprises.
 * Tests self-register; the runner prints one line per test and returns
 * non-zero on any failure. */
#ifndef AV_TEST_H
#define AV_TEST_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef void (*av_test_fn)(void);

typedef struct av_test_case {
    const char *suite;
    const char *name;
    av_test_fn  fn;
    struct av_test_case *next;
} av_test_case;

void av_test_register(av_test_case *tc);
void av_test_fail(const char *file, int line, const char *fmt, ...);
int  av_test_run_all(int argc, char **argv);

#define TEST(suite_, name_)                                                    \
    static void av_test_##suite_##_##name_(void);                              \
    static av_test_case av_tc_##suite_##_##name_ = {                           \
        #suite_, #name_, av_test_##suite_##_##name_, NULL };                   \
    __attribute__((constructor)) static void av_reg_##suite_##_##name_(void) { \
        av_test_register(&av_tc_##suite_##_##name_);                           \
    }                                                                          \
    static void av_test_##suite_##_##name_(void)

#define CHECK(cond)                                                            \
    do { if (!(cond)) av_test_fail(__FILE__, __LINE__, "CHECK(%s)", #cond); } while (0)

#define CHECK_EQ(a, b)                                                         \
    do { long long _a = (long long)(a), _b = (long long)(b);                   \
         if (_a != _b) av_test_fail(__FILE__, __LINE__,                        \
             "CHECK_EQ(%s, %s): %lld != %lld", #a, #b, _a, _b); } while (0)

#define CHECK_STATUS(expr, want)                                               \
    do { av_status_t _s = (expr);                                              \
         if (_s != (want)) av_test_fail(__FILE__, __LINE__,                    \
             "%s: got status %d, want %d", #expr, (int)_s, (int)(want)); } while (0)

#endif /* AV_TEST_H */
