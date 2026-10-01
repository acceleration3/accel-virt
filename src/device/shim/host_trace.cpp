#include "device/shim/host_trace.hpp"

#include <sys/prctl.h>

#include "core/trace/event_ring_layout.hpp"
#include "core/trace/function_scope.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_layout.hpp"

#include <link.h>


/* Freestanding: the declaration <stdlib.h> would give, for libc's own. */
extern "C" char *getenv(const char *name);

namespace av {

namespace {

struct Search {
    uint64_t address;
    uint64_t low;
    uint64_t high;
};

/* dl_iterate_phdr's callback: the loaded object whose segments hold the
 * address, as the span from its lowest to its highest loaded byte. */
int find_object(dl_phdr_info *info, size_t, void *context)
{
    AV_SCOPE("find_object");
    auto *const search = static_cast<Search *>(context);
    uint64_t    low = ~uint64_t{0};
    uint64_t    high = 0u;
    bool        holds = false;
    for (unsigned index = 0u; index < info->dlpi_phnum; index++) {
        const ElfW(Phdr) &header = info->dlpi_phdr[index];
        if (header.p_type != PT_LOAD) continue;
        const uint64_t start = info->dlpi_addr + header.p_vaddr;
        const uint64_t end = start + header.p_memsz;
        low = start < low ? start : low;
        high = end > high ? end : high;
        holds = holds || (search->address >= start && search->address < end);
    }
    if (!holds) return 0;
    search->low = low;
    search->high = high;
    return 1;
}

}  // namespace

HostTrace::HostTrace() : host_(), region_(), table_(host_), events_region_(), events_(), image_(0u, 0u)
{
    AV_SCOPE("HostTrace::HostTrace");
}

Status HostTrace::start(const char *module)
{
    AV_SCOPE("HostTrace::start");
    if (started()) return Status::invalid_state;
    const size_t bytes = TraceLayout(slots).bytes();
    const Status opened = region_.open(module, bytes);
    if (opened != Status::ok) return opened;
    /* This function's own address names the object this copy of the code is
     * linked into: QEMU's module, or the executor library. */
    Search search{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&find_object)), 0u, 0u};
    if (dl_iterate_phdr(&find_object, &search) != 0) image_ = ImageRange(search.low, search.high - search.low);
    const Status laid = table_.initialize(region_.region(), region_.bytes(), module, region_.process());
    if (laid != Status::ok) return laid;
    /* A profiler attaching to a QEMU this shell did not start is refused under
     * Yama's ptrace_scope 1 unless the process names it allowed. */
    const char *const ptrace = getenv("AV_PTRACE");
    if (ptrace != nullptr && ptrace[0] == '1') prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    /* Every scope costs the call it opens - a batch runs a few hundred - so
     * the table is bound only when a trace is asked for, as the kernel's is. */
    const char *const scopes = getenv("AV_TRACE_SCOPES");
    const char *const perfetto = getenv("AV_PERFETTO");
    if ((scopes != nullptr && scopes[0] == '1') || (perfetto != nullptr && perfetto[0] == '1'))
        scope_binding.bind(table_);
    /* A timeline for Perfetto keeps the outer calls only, unless told
     * otherwise: every scope reads the clock twice, and the hottest leaves
     * ran ten million a second. The device's batch loop is kept (its idle
     * spin is below it); the executor down to each command's handler and
     * what it calls. AV_TRACE_DEPTH sets both; 0 records everything. */
    constexpr uint32_t device_depth = 2u;
    constexpr uint32_t executor_depth = 6u;
    const bool         device = module[0] == 'd';
    const char *const  depth = getenv("AV_TRACE_DEPTH");
    uint32_t           limit = perfetto == nullptr || perfetto[0] != '1' ? 0u : device ? device_depth : executor_depth;
    if (depth != nullptr) {
        limit = 0u;
        for (const char *digit = depth; *digit >= '0' && *digit <= '9'; digit++)
            limit = limit * 10u + static_cast<uint32_t>(*digit - '0');
    }
    table_.limit_depth(limit);
    /* AV_TRACE_ONLY: a comma-separated list of scope names, the only ones
     * recorded. */
    table_.only(getenv("AV_TRACE_ONLY"));
    /* A ring per slot the size of a busy thread's second or so; the file is
     * sparse, so only the slots threads use take memory. */
    constexpr uint32_t events_per_slot = 1u << 22u;
    const char *const  wanted = getenv("AV_PERFETTO");
    if (wanted != nullptr && wanted[0] == '1' &&
        events_region_.open("/av-events-", module, EventRingLayout(slots, events_per_slot).bytes()) == Status::ok &&
        events_.initialize(events_region_.region(), events_region_.bytes(), slots, events_per_slot) == Status::ok)
    {
        host_.precise();
        table_.record_events(events_);
    }
    return Status::ok;
}

bool HostTrace::started() const
{
    AV_SCOPE("HostTrace::started");
    return region_.region() != nullptr;
}

TraceTable &HostTrace::table()
{
    AV_SCOPE("HostTrace::table");
    return table_;
}

const ImageRange &HostTrace::image() const
{
    AV_SCOPE("HostTrace::image");
    return image_;
}

const TraceHost &HostTrace::host() const
{
    AV_SCOPE("HostTrace::host");
    return host_;
}

const uint8_t *HostTrace::region() const
{
    AV_SCOPE("HostTrace::region");
    return region_.region();
}

size_t HostTrace::bytes() const
{
    AV_SCOPE("HostTrace::bytes");
    return region_.bytes();
}

uint32_t HostTrace::process() const
{
    AV_SCOPE("HostTrace::process");
    return region_.process();
}

}  // namespace av
