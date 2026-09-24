#include "core/trace/function_scope.hpp"

#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_table.hpp"

namespace av {

void FunctionScope::enter(const ScopeName &name)
{
    slot_ = table_->enter(name);
}

void FunctionScope::leave()
{
    if (slot_ != TraceTable::no_slot) table_->leave(slot_);
}

}  // namespace av
