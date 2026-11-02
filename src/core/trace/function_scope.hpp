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
    void enter(const ScopeName &name);
    void leave();

    TraceTable *table_;
    uint32_t    slot_;
};

inline FunctionScope::FunctionScope(const ScopeBinding &binding, const ScopeName &name)
    : table_(binding.table()), slot_(0u)
{
    if (table_ != nullptr) enter(name);
}

inline FunctionScope::~FunctionScope()
{
    if (table_ != nullptr) leave();
}

}

#define AV_SCOPE(name)                                             \
    static constexpr ::av::ScopeName av_scope_name_{name};         \
    const ::av::FunctionScope av_scope_{::av::scope_binding, av_scope_name_}

#endif
