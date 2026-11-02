#include "core/platform/lock_marks.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_table.hpp"

namespace av {

LockMarks::LockMarks(const void *lock) : lock_(static_cast<uint64_t>(reinterpret_cast<uintptr_t>(lock)))
{
    AV_SCOPE("LockMarks::LockMarks");
}

void LockMarks::waiting() const
{
    AV_SCOPE("LockMarks::waiting");
    if (TraceTable *const table = scope_binding.table(); table != nullptr) table->waiting(lock_);
}

void LockMarks::acquired() const
{
    AV_SCOPE("LockMarks::acquired");
    if (TraceTable *const table = scope_binding.table(); table != nullptr) table->acquired(lock_);
}

void LockMarks::released() const
{
    AV_SCOPE("LockMarks::released");
    if (TraceTable *const table = scope_binding.table(); table != nullptr) table->released(lock_);
}

void LockMarks::woken() const
{
    AV_SCOPE("LockMarks::woken");
    if (TraceTable *const table = scope_binding.table(); table != nullptr) table->waiting(0u);
}

}
