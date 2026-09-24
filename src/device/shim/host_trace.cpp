#include "device/shim/host_trace.hpp"

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
    /* Every scope costs the call it opens - a batch runs a few hundred - so
     * the table is bound only when a trace is asked for, as the kernel's is. */
    const char *const scopes = getenv("RVK_TRACE_SCOPES");
    const char *const perfetto = getenv("RVK_PERFETTO");
    if ((scopes != nullptr && scopes[0] == '1') || (perfetto != nullptr && perfetto[0] == '1'))
        scope_binding.bind(table_);
    /* A ring per slot the size of a busy thread's second or so; the file is
     * sparse, so only the slots threads use take memory. */
    constexpr uint32_t events_per_slot = 1u << 20u;
    const char *const  wanted = getenv("RVK_PERFETTO");
    if (wanted != nullptr && wanted[0] == '1' &&
        events_region_.open("/rvk-events-", module, EventRing::bytes_for(slots, events_per_slot)) == Status::ok &&
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
