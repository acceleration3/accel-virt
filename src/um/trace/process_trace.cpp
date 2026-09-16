#include "um/trace/process_trace.hpp"

#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"

#include <windows.h>

namespace av {

ProcessTrace::ProcessTrace()
    : host_(), security_(), section_(), table_(host_), service_(), crash_(), module_(""), previous_(nullptr)
{
    AV_SCOPE("ProcessTrace::ProcessTrace");
}

Status ProcessTrace::start(const char *module, StateReporter &reporter, Filter filter)
{
    AV_SCOPE("ProcessTrace::start");
    const uint32_t pid = GetCurrentProcessId();
    module_ = module;
    Status status = section_.create(module, pid, security_);
    if (status != Status::ok) return status;
    status = table_.initialize(section_.region(), section_.bytes(), module, pid);
    if (status != Status::ok) return status;
    scope_binding.bind(table_);
    HMODULE     pinned = nullptr;
    const DWORD pin = GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS;
    if (!GetModuleHandleExW(pin, reinterpret_cast<LPCWSTR>(this), &pinned)) return Status::invalid_state;
    previous_ = reinterpret_cast<Filter>(SetUnhandledExceptionFilter(reinterpret_cast<LPTOP_LEVEL_EXCEPTION_FILTER>(filter)));
    return service_.start(module, pid, reporter, security_);
}

long ProcessTrace::crashed(void *pointers)
{
    AV_SCOPE("ProcessTrace::crashed");
    if (table_.region() != nullptr)
        crash_.write(module_, table_, host_, pointers, static_cast<uint64_t>(reinterpret_cast<uintptr_t>(this)));
    return previous_ != nullptr ? previous_(pointers) : EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace av
