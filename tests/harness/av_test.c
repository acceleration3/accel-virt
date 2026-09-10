#include "av_test.h"
#include <stdarg.h>
#include <stdlib.h>

static av_test_case *g_head;
static int g_failed_current;
static int g_failures;

void av_test_register(av_test_case *tc) { tc->next = g_head; g_head = tc; }

void av_test_fail(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "\n    %s:%d: ", file, line);
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n");
    g_failed_current = 1;
}

/* Reset hooks the platform mock installs so each test starts clean. */
void av_mock_reset(void);
int  av_mock_leaks(void);

int av_test_run_all(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;
    int run = 0;
    for (av_test_case *tc = g_head; tc; tc = tc->next) {
        if (filter && !strstr(tc->suite, filter) && !strstr(tc->name, filter))
            continue;
        g_failed_current = 0;
        av_mock_reset();
        printf("%-28s %-52s", tc->suite, tc->name);
        fflush(stdout);
        tc->fn();
        int leaked = av_mock_leaks();
        if (leaked) {
            av_test_fail(__FILE__, __LINE__, "%d allocation(s) leaked", leaked);
        }
        printf("%s\n", g_failed_current ? "FAIL" : "ok");
        g_failures += g_failed_current;
        run++;
    }
    printf("\n%d test(s), %d failure(s)\n", run, g_failures);
    return g_failures != 0;
}

int main(int argc, char **argv) { return av_test_run_all(argc, argv); }
