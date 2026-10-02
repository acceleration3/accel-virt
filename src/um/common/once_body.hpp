/* What RunOnce runs: the body the first caller runs while every other caller
 * waits for it. */
#ifndef AV_UM_ONCE_BODY_HPP
#define AV_UM_ONCE_BODY_HPP

namespace av {

class OnceBody {
public:
    virtual void run_once() = 0;

protected:
    constexpr OnceBody() = default;
    /* Non-virtual: nothing is destroyed through this pointer (CLAUDE.md,
     * Adaptations). */
    ~OnceBody() = default;
};

}  // namespace av

#endif  // AV_UM_ONCE_BODY_HPP
