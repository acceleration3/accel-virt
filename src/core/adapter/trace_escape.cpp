#include "core/adapter/trace_escape.hpp"

#include "core/platform/platform.hpp"
#include "core/support/scope_exit.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/state_reporter.hpp"
#include "core/trace/state_writer.hpp"
#include "core/trace/trace_table.hpp"
#include "proto/trace/trace_escape_record.hpp"

namespace av {

TraceEscape::TraceEscape(Platform &platform, StateReporter &reporter) : platform_(platform), reporter_(reporter)
{
    AV_SCOPE("TraceEscape::TraceEscape");
}

Status TraceEscape::answer(uint8_t *bytes, size_t length)
{
    AV_SCOPE("TraceEscape::answer");
    platform_.require_irql_at_most(Irql::passive, "TraceEscape::answer");
    TraceEscapeRecord request;
    if (request.decode(bytes, length) != Status::ok) return Status::malformed;
    if (request.kind() == TraceEscapeRecord::Kind::table) {
        const TraceTable *const table = scope_binding.table();
        reply(bytes, length, request, table != nullptr ? table->region() : nullptr,
              table != nullptr ? table->bytes() : 0u);
        return Status::ok;
    }
    auto *const scratch = static_cast<char *>(platform_.allocate(state_bytes, Pool::non_paged));
    if (scratch == nullptr) return Status::no_memory;
    ScopeExit   release([&] { platform_.release(scratch); });
    StateWriter writer(scratch, state_bytes);
    reporter_.report(writer);
    reply(bytes, length, request, static_cast<const uint8_t *>(static_cast<const void *>(scratch)), writer.length());
    return Status::ok;
}

void TraceEscape::reply(uint8_t *bytes, size_t length, const TraceEscapeRecord &request, const uint8_t *source,
                        uint64_t total)
{
    AV_SCOPE("TraceEscape::reply");
    const uint64_t left = request.offset() < total ? total - request.offset() : 0u;
    const uint32_t filled = left < request.window() ? static_cast<uint32_t>(left) : request.window();
    if (filled != 0u)
        platform_.copy(bytes + TraceEscapeRecord::encoded_size, source + request.offset(), filled);
    /* The request was decoded from these bytes, so they hold a record. */
    static_cast<void>(request.answered(total, filled, platform_.interrupt_time()).encode(bytes, length));
}

}  // namespace av
