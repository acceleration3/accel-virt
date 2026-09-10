/* The platform, as an injected object rather than a set of free functions.
 *
 * Two implementations exist, which is why the abstraction is not speculative:
 * KernelPlatform (src/km) and MockPlatform (tests/harness). Every system class
 * takes a Platform& at construction; nothing reaches for a global. */
#ifndef AV_PLATFORM_HPP
#define AV_PLATFORM_HPP

#include <cstddef>
#include <cstdint>

namespace av {

enum class Irql : unsigned { passive = 0, apc = 1, dispatch = 2, isr = 3 };
enum class Pool : unsigned { non_paged = 0, paged = 1 };

/* Status codes. Chosen so the shim maps them 1:1 onto NTSTATUS without core
 * knowing what an NTSTATUS is. */
enum class Status : unsigned {
    ok = 0,
    invalid_parameter,
    no_memory,
    not_found,
    conflict,
    no_space,
    not_supported,
    invalid_state,
    too_large
};

class Platform {
public:
    Platform() = default;
    virtual ~Platform() = default;
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;

    virtual void *allocate(std::size_t bytes, Pool pool) = 0;
    virtual void  release(void *block) = 0;

    virtual void zero(void *dst, std::size_t bytes) = 0;
    virtual void copy(void *dst, const void *src, std::size_t bytes) = 0;

    virtual Irql current_irql() const = 0;

    /* Declare the ceiling a function may be called at. The kernel
     * implementation bugchecks; the mock records the violation so it can be
     * unit tested. */
    virtual void require_irql_at_most(Irql ceiling, const char *function) = 0;

    virtual void log(const char *message) = 0;

    /* Reported when an invariant fails inside a checked build. */
    virtual void report_broken_invariant(const char *what,
                                         const char *function) = 0;
};

/* Runs a callable when the scope exits. The reason this project is C++ and not
 * C: a fallible function that returns early cannot forget to release, so
 * cleanup stops being a branch that has to be covered. */
template <typename F>
class ScopeExit {
public:
    explicit ScopeExit(F action) : action_(action) {}
    ~ScopeExit() { if (armed_) action_(); }
    ScopeExit(const ScopeExit &) = delete;
    ScopeExit &operator=(const ScopeExit &) = delete;
    void dismiss() { armed_ = false; }

private:
    F    action_;
    bool armed_ = true;
};

/* Asserts an object's invariants on construction and again on destruction, so
 * a function that mutates state proves it left the object consistent no matter
 * which return it took. Compiled out of release builds. */
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

/* A branch that cannot be reached from a test. Counted by the coverage gate;
 * every use must be justified in review. */
#define AV_UNTESTABLE(reason) /* reason */

}  // namespace av

#endif  // AV_PLATFORM_HPP
