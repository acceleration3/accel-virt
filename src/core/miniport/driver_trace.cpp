#include "core/miniport/driver_trace.hpp"

#include "core/support/text_buffer.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_host.hpp"
#include "core/trace/trace_layout.hpp"
#include "core/trace/trace_snapshot.hpp"
#include "core/trace/trace_text.hpp"

namespace av {

DriverTrace::DriverTrace(Platform &platform, TraceHost &host, const ImageRange &image)
    : platform_(platform), host_(host), table_(host), image_(image), region_(nullptr), bytes_(0u),
      registered_(false)
{
    AV_SCOPE("DriverTrace::DriverTrace");
}

void DriverTrace::start(BugcheckRoutine routine)
{
    AV_SCOPE("DriverTrace::start");
    platform_.require_irql_at_most(Irql::passive, "DriverTrace::start");
    if (region_ != nullptr) return;
    const size_t bytes = TraceLayout(slots).bytes();
    auto *const region = static_cast<uint8_t *>(platform_.allocate_contiguous(bytes));
    if (region == nullptr) {
        platform_.log("avgpu: trace table not allocated; tracing is off");
        return;
    }
    region_ = region;
    bytes_ = bytes;
    /* Contiguous memory is page-aligned and this size holds every slot, so the
     * table cannot refuse it. */
    static_cast<void>(table_.initialize(region_, bytes_, "avgpu", 0u));
    scope_binding.bind(table_);
    char       line[line_bytes];
    TextBuffer message(line, sizeof line);
    message.append("avgpu: trace table at physical 0x");
    message.append_hex(platform_.physical_address(region_), 16u);
    message.append(" bytes ");
    message.append_decimal(bytes_);
    platform_.log(message.text());
    if (routine == nullptr) return;
    registered_ = platform_.register_bugcheck(region_, bytes_, routine);
    if (!registered_) platform_.log("avgpu: trace table bugcheck callback refused");
}

void DriverTrace::stop()
{
    AV_SCOPE("DriverTrace::stop");
    platform_.require_irql_at_most(Irql::passive, "DriverTrace::stop");
    if (region_ == nullptr) return;
    if (scope_binding.table() == &table_) scope_binding.unbind();
    /* The scopes this call runs in - and its callers' - leave after the region
     * is gone. */
    table_.withdraw();
    if (registered_) platform_.deregister_bugcheck();
    registered_ = false;
    platform_.release_contiguous(region_, bytes_);
    region_ = nullptr;
    bytes_ = 0u;
}

void DriverTrace::bugcheck()
{
    AV_SCOPE("DriverTrace::bugcheck");
    if (region_ == nullptr) return;
    TraceSnapshot snapshot;
    if (snapshot.read(region_, bytes_) != Status::ok) {
        platform_.log_at_bugcheck("avgpu: trace table unreadable at the bugcheck");
        return;
    }
    TraceText text(snapshot, image_, host_.clock());
    for (;;) {
        char       line[line_bytes];
        TextBuffer out(line, sizeof line);
        out.append("avgpu: ");
        if (!text.next(out)) return;
        platform_.log_at_bugcheck(out.text());
    }
}

/* A region of the table's size is held exactly while tracing runs, and only a
 * started table can be registered. */
bool DriverTrace::invariants_hold() const
{
    AV_SCOPE("DriverTrace::invariants_hold");
    if (region_ == nullptr) return bytes_ == 0u && !registered_;
    return bytes_ == TraceLayout(slots).bytes();
}

}  // namespace av
