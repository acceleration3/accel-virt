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

}

#endif
