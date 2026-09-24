#include "core/trace/scope_binding.hpp"

namespace av {

constinit ScopeBinding scope_binding;

void        ScopeBinding::bind(TraceTable &table) { table_ = &table; }
void        ScopeBinding::unbind() { table_ = nullptr; }

}  // namespace av
