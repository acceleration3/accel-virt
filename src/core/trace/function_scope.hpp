/* One function's frame on the calling thread's trace stack, for as long as the
 * function runs: pushed at construction, popped at destruction. AV_SCOPE is
 * the one way to make one, as the first line of every function in src/
 * (docs/trace.md, tools/scope_marks.py). */
#ifndef AV_FUNCTION_SCOPE_HPP
#define AV_FUNCTION_SCOPE_HPP

#include "core/trace/scope_binding.hpp"
#include "core/trace/scope_name.hpp"

#include <stdint.h>

namespace av {

class TraceTable;

class FunctionScope {
public:
    FunctionScope(const ScopeBinding &binding, const ScopeName &name);
    ~FunctionScope();
    FunctionScope(const FunctionScope &) = delete;
    FunctionScope &operator=(const FunctionScope &) = delete;

private:
    TraceTable *table_;
    uint32_t    slot_;
};

}  // namespace av

/* The name is the function's qualified name as written, so it greps to the
 * definition; its id is computed while compiling. */
#define AV_SCOPE(name)                                             \
    static constexpr ::av::ScopeName av_scope_name_{name};         \
    const ::av::FunctionScope av_scope_{::av::scope_binding, av_scope_name_}

#endif  // AV_FUNCTION_SCOPE_HPP
