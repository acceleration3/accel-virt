#include "core/trace/function_scope.hpp"

#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_table.hpp"

namespace av {

FunctionScope::FunctionScope(const ScopeBinding &binding, const ScopeName &name)
    : table_(binding.table()), slot_(TraceTable::no_slot)
{
    if (table_ != nullptr) slot_ = table_->enter(name);
}

FunctionScope::~FunctionScope()
{
    if (table_ != nullptr && slot_ != TraceTable::no_slot) table_->leave(slot_);
}

}  // namespace av
