/* Asserts an object's invariants on construction and again on destruction, so
 * a function that mutates state proves it left the object consistent no matter
 * which return it took. Compiled out of release builds. */
#ifndef AV_INVARIANT_SCOPE_HPP
#define AV_INVARIANT_SCOPE_HPP

#include "core/platform/platform.hpp"

namespace av {

template <typename T>
class InvariantScope {
public:
#if defined(AV_CHECKED)
    InvariantScope(Platform &platform, const T &subject, const char *function)
        : platform_(&platform), subject_(&subject), function_(function)
    {
        verify();
    }
    ~InvariantScope() { verify(); }
#else
    InvariantScope(Platform &, const T &, const char *) {}
    ~InvariantScope() = default;
#endif
    InvariantScope(const InvariantScope &) = delete;
    InvariantScope &operator=(const InvariantScope &) = delete;

private:
#if defined(AV_CHECKED)
    void verify() const
    {
        if (!subject_->invariants_hold())
            platform_->report_broken_invariant("invariants_hold", function_);
    }
    Platform   *platform_;
    const T    *subject_;
    const char *function_;
#endif
};

}  // namespace av

#endif  // AV_INVARIANT_SCOPE_HPP
