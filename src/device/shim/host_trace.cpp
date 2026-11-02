#include "device/shim/host_trace.hpp"

#include <sys/prctl.h>

#include "core/trace/event_ring_layout.hpp"
#include "core/trace/function_scope.hpp"
#include "device/host/env_setting.hpp"
#include "core/trace/scope_binding.hpp"
#include "core/trace/trace_layout.hpp"

#include <link.h>


namespace av {

namespace {

struct Search {
    uint64_t address;
    uint64_t low;
    uint64_t high;
};

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

}

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
    Search search{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&find_object)), 0u, 0u};
    if (dl_iterate_phdr(&find_object, &search) != 0) image_ = ImageRange(search.low, search.high - search.low);
    const Status laid = table_.initialize(region_.region(), region_.bytes(), module, region_.process());
    if (laid != Status::ok) return laid;
    if (EnvSetting("AV_PTRACE").on()) prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    const bool perfetto = EnvSetting("AV_PERFETTO").on();
    if (perfetto || EnvSetting("AV_TRACE_SCOPES").on()) scope_binding.bind(table_);
    constexpr uint32_t device_depth = 2u;
    constexpr uint32_t executor_depth = 6u;
    const bool         device = module[0] == 'd';
    const EnvSetting   depth("AV_TRACE_DEPTH");
    uint64_t           limit = !perfetto ? 0u : device ? device_depth : executor_depth;
    if (depth.text() != nullptr && !depth.number(limit)) limit = 0u;
    table_.limit_depth(static_cast<uint32_t>(limit));
    table_.only(EnvSetting("AV_TRACE_ONLY").text());
    constexpr uint32_t events_per_slot = 1u << 22u;
    if (perfetto &&
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

}
