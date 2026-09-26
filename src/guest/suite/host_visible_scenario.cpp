#include "guest/suite/host_visible_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/memory_segment_group.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

HostVisibleScenario::HostVisibleScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "host_visible"), pattern_(pattern_bytes),
      matched_(0u)
      {
          AV_SCOPE("HostVisibleScenario::HostVisibleScenario");
      }

void HostVisibleScenario::run()
{
    AV_SCOPE("HostVisibleScenario::run");
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession           session(kmt_, search_.adapter());
    /* Host memory, which VidMm places in the CPU-visible segment when the
     * device offers one: the pattern then lands in the BAR, where the host
     * reads it back (run_guest_suite.py's hostvisible step). */
    const ClientDescriptor descriptor(measured_bytes, AllocationKind::host_memory);
    uint32_t               index = 0u;
    void                  *view = nullptr;
    Status                 status = steps_.check("device", session.open_device());
    if (status == Status::ok)
        status = steps_.check("allocation", session.allocate(descriptor.bytes(), descriptor.size(), index));
    /* Resident first: VidMm keeps an allocation it has never placed in a
     * system-memory backing store, and a lock of it maps that. */
    uint64_t address = 0u;
    if (status == Status::ok) status = steps_.check("paging_queue", session.open_paging_queue());
    if (status == Status::ok) status = steps_.check("resident", session.map(index, descriptor.pages(), address));
    if (status == Status::ok) {
        /* VidMm keeps a memory segment's allocations inside the process's local
         * budget: one of zero sends every allocation to the aperture. In KiB. */
        constexpr uint64_t kib = 1024u;
        uint64_t           budget = 0u, usage = 0u;
        const Status local = kmt_.query_video_memory(search_.adapter(), MemorySegmentGroup::local, budget, usage);
        static_cast<void>(steps_.check_measured("local_budget_kib", local, static_cast<uint32_t>(budget / kib)));
        static_cast<void>(steps_.check_measured("local_usage_kib", local, static_cast<uint32_t>(usage / kib)));
        const Status non_local =
            kmt_.query_video_memory(search_.adapter(), MemorySegmentGroup::non_local, budget, usage);
        static_cast<void>(steps_.check_measured("non_local_budget_kib", non_local, static_cast<uint32_t>(budget / kib)));
        static_cast<void>(steps_.check_measured("non_local_usage_kib", non_local, static_cast<uint32_t>(usage / kib)));
    }
    if (status == Status::ok) status = steps_.check("lock", session.lock(index, view));
    if (status == Status::ok) {
        measure(static_cast<volatile uint64_t *>(view));
        auto *const memory = static_cast<volatile unsigned char *>(view);
        pattern_.write(memory);
        matched_ = pattern_.matched(memory);
        static_cast<void>(steps_.check_measured("pattern", Status::ok, matched_));
        steps_.check("unlock", session.unlock(index));
    }
    steps_.check_teardown(session);
}

void HostVisibleScenario::measure(volatile uint64_t *view)
{
    AV_SCOPE("HostVisibleScenario::measure");
    constexpr uint64_t words = measured_bytes / sizeof(uint64_t);
    uint64_t           frequency = 0u;
    /* The first pass pays for every page the host has not backed yet; the
     * second is what a process that reuses its memory sees. */
    const uint64_t     first_began = kmt_.performance_counter(frequency);
    for (uint64_t index = 0u; index < words; index++) view[index] = 0u;
    const uint64_t first_ticks = kmt_.performance_counter(frequency) - first_began;
    const uint64_t write_began = kmt_.performance_counter(frequency);
    for (uint64_t index = 0u; index < words; index++) view[index] = index;
    const uint64_t write_ticks = kmt_.performance_counter(frequency) - write_began;
    static_cast<void>(steps_.check_measured("first_write_mbps", Status::ok, megabytes_per_second(first_ticks, frequency)));
    uint64_t       sum = 0u;
    const uint64_t read_began = kmt_.performance_counter(frequency);
    for (uint64_t index = 0u; index < words; index++) sum += view[index];
    const uint64_t read_ticks = kmt_.performance_counter(frequency) - read_began;
    const Status   read_back = sum == words * (words - 1u) / 2u ? Status::ok : Status::malformed;
    static_cast<void>(steps_.check_measured("write_mbps", Status::ok, megabytes_per_second(write_ticks, frequency)));
    static_cast<void>(steps_.check_measured("read_mbps", read_back, megabytes_per_second(read_ticks, frequency)));
}

uint32_t HostVisibleScenario::megabytes_per_second(uint64_t ticks, uint64_t frequency) const
{
    AV_SCOPE("HostVisibleScenario::megabytes_per_second");
    constexpr uint64_t megabyte = 1000000u;
    if (ticks == 0u) return 0u;
    return static_cast<uint32_t>(measured_bytes * frequency / ticks / megabyte);
}

uint32_t HostVisibleScenario::matched_bytes() const
{
    AV_SCOPE("HostVisibleScenario::matched_bytes");
    return matched_;
}

}  // namespace av
