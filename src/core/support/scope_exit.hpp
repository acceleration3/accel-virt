/* Runs a callable when the scope exits. The reason this project is C++ and not
 * C: a fallible function that returns early cannot forget to release, so
 * cleanup stops being a branch that has to be covered. */
#ifndef AV_SCOPE_EXIT_HPP
#define AV_SCOPE_EXIT_HPP

#include "core/trace/function_scope.hpp"

namespace av {

template <typename F>
class ScopeExit {
public:
    explicit ScopeExit(F action) : action_(action)
    {
        AV_SCOPE("ScopeExit::ScopeExit");
    }
    ~ScopeExit()
    {
        AV_SCOPE("ScopeExit::~ScopeExit");
        if (armed_) action_();
    }
    ScopeExit(const ScopeExit &) = delete;
    ScopeExit &operator=(const ScopeExit &) = delete;
    void dismiss()
    {
        AV_SCOPE("ScopeExit::dismiss");
        armed_ = false;
    }

private:
    F    action_;
    bool armed_ = true;
};

}  // namespace av

#endif  // AV_SCOPE_EXIT_HPP
