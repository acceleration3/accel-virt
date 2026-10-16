#include "um/trace/process_trace.hpp"

#include "core/trace/event_ring_layout.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"

#include <windows.h>

namespace av {

ProcessTrace::ProcessTrace()
    : host_(), security_(), section_(), table_(host_), directory_(), events_file_(), events_(), service_(), crash_(), module_(""), previous_(nullptr)
{
    AV_SCOPE("ProcessTrace::ProcessTrace");
}

/* A timeline of this process for Perfetto, when one is being recorded: every
 * scope an event in a file the host pulls, stamped by the performance
 * counter the host's batches carry too. */
void ProcessTrace::record_events(const char *module, uint32_t pid)
{
    AV_SCOPE("ProcessTrace::record_events");
    constexpr uint64_t hundred_ns = 10000000u;
    if (!events_file_.wanted()) return;
    const uint32_t events_per_slot = events_file_.events_per_slot();
    const size_t bytes = EventRingLayout(TraceSection::slots, events_per_slot).bytes();
    if (events_file_.open(module, pid, bytes) != Status::ok) return;
    if (events_.initialize(events_file_.region(), bytes, TraceSection::slots, events_per_slot) != Status::ok) return;
    host_.precise();
    events_.describe_clock(hundred_ns);
    table_.record_events(events_);
    table_.limit_depth(events_file_.depth());
    events_file_.only(table_);
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
    /* Every scope costs the call it opens, thousands a frame on an
     * application's own thread, so the table is bound only when a trace is
     * asked for: C:\ProgramData\av\trace-scopes, or the perfetto flag. */
    if (directory_.flagged(L"trace-scopes") || events_file_.wanted())
        scope_binding.bind(table_);
    record_events(module, pid);
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
