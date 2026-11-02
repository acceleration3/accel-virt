#ifndef AV_UM_RUN_ONCE_HPP
#define AV_UM_RUN_ONCE_HPP

namespace av {

class OnceBody;

class RunOnce {
public:
    constexpr RunOnce();
    RunOnce(const RunOnce &) = delete;
    RunOnce &operator=(const RunOnce &) = delete;

    [[nodiscard]] bool run(OnceBody &body);

private:
    void *once_;
};

constexpr RunOnce::RunOnce() : once_(nullptr) {}

}

#endif
