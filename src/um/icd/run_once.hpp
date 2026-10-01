/* A body run once in the process by whichever thread calls first, every
 * other caller waiting until it has run: InitOnceExecuteOnce. The INIT_ONCE
 * is one pointer and zero when untouched (INIT_ONCE_STATIC_INIT), kept
 * opaque so windows.h stays out of the headers that hold one. */
#ifndef AV_UM_RUN_ONCE_HPP
#define AV_UM_RUN_ONCE_HPP

namespace av {

class OnceBody;

class RunOnce {
public:
    /* Constexpr: it lives in a DLL's constinit storage. */
    constexpr RunOnce();
    RunOnce(const RunOnce &) = delete;
    RunOnce &operator=(const RunOnce &) = delete;

    /* Runs `body` unless it has run; false when the kernel could not. */
    [[nodiscard]] bool run(OnceBody &body);

private:
    void *once_;
};

constexpr RunOnce::RunOnce() : once_(nullptr) {}

}  // namespace av

#endif  // AV_UM_RUN_ONCE_HPP
