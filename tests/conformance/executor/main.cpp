/* executor_conformance <library> ...
 *
 * Every library named is held to src/device/abi/rvk_executor.h. A suite that
 * only one library passes has tested nothing, so the build runs it against the
 * echo library as well as this project's. */
#include "conformance/executor/executor_conformance.hpp"

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace {

/* Nothing in the device times out a call into an executor, so a library that
 * never returns hangs QEMU at CTX_DESTROY. The suite must find that out rather
 * than become the thing that hangs. */
constexpr unsigned watchdog_seconds = 30u;

extern "C" void give_up(int)
{
    static const char message[] = "executor_conformance: a call did not return\n";
    (void)!write(2, message, sizeof message - 1u);
    _exit(1);
}

}  // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::printf("usage: executor_conformance <library> [library...]\n");
        return 2;
    }
    struct sigaction watchdog = {};
    watchdog.sa_handler = give_up;
    (void)sigaction(SIGALRM, &watchdog, nullptr);

    int failed = 0;
    for (int i = 1; i < argc; i++) {
        av::ExecutorConformance conformance;
        (void)alarm(watchdog_seconds);
        const bool passed = conformance.run(argv[i]);
        (void)alarm(0u);

        for (std::uint32_t f = 0u; f < conformance.findings(); f++)
            std::fprintf(stderr, "executor_conformance: %s: %s\n", argv[i], conformance.finding(f));
        for (std::uint32_t s = 0u; s < conformance.skipped(); s++)
            std::printf("executor_conformance: %s: skipped: %s\n", argv[i], conformance.skip(s));
        std::printf("executor_conformance: %s: %s, %u statements checked\n", argv[i],
                    passed ? "ok" : "FAILED", conformance.checked());
        if (!passed) failed = 1;
        /* A run that checks nothing must not read as a pass. */
        if (conformance.checked() == 0u) {
            std::fprintf(stderr, "executor_conformance: %s: checked nothing\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
