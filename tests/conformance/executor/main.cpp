#include "conformance/executor/executor_conformance.hpp"

#include <csignal>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

extern "C" const char *__lsan_default_suppressions(void);
extern "C" const char *__lsan_default_suppressions(void)
{
    return "leak:libvulkan\n"
           "leak:libOpenCL\n"
           "leak:intelocl\n"
           "leak:libigdrcl\n"
           "leak:libnvidia\n"
           "leak:nvidia_icd\n"
           "leak:libGLX\n"
           "leak:libEGL\n"
           "leak:libgallium\n"
           "leak:iris_dri\n"
           "leak:libdbus-1\n"
           "leak:libz\n";
}

namespace {
constexpr unsigned watchdog_seconds = 30u;

extern "C" void give_up(int)
{
    static const char message[] = "executor_conformance: a call did not return\n";
    (void)!write(2, message, sizeof message - 1u);
    _exit(1);
}

}

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::printf("usage: executor_conformance [--gpu <32 hex>] <library> [library...]\n");
        return 2;
    }
    const char *gpu = nullptr;
    int first = 1;
    if (argc >= 3 && std::strcmp(argv[1], "--gpu") == 0) {
        gpu = argv[2];
        first = 3;
    }
    struct sigaction watchdog = {};
    watchdog.sa_handler = give_up;
    (void)sigaction(SIGALRM, &watchdog, nullptr);

    int failed = 0;
    for (int i = first; i < argc; i++) {
        av::ExecutorConformance conformance;
        if (gpu != nullptr && !conformance.use_gpu(gpu)) {
            std::fprintf(stderr, "executor_conformance: --gpu wants 32 hex characters\n");
            return 2;
        }
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
        if (conformance.checked() == 0u) {
            std::fprintf(stderr, "executor_conformance: %s: checked nothing\n", argv[i]);
            failed = 1;
        }
    }
    return failed;
}
