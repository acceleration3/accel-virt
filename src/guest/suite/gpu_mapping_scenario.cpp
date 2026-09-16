#include "guest/suite/gpu_mapping_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

namespace {

constexpr uint64_t allocation_bytes = 65536u;

}  // namespace

GpuMappingScenario::GpuMappingScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "mapping")
    {
        AV_SCOPE("GpuMappingScenario::GpuMappingScenario");
    }

void GpuMappingScenario::run()
{
    AV_SCOPE("GpuMappingScenario::run");
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession           session(kmt_, search_.adapter());
    const ClientDescriptor descriptor(allocation_bytes);
    uint32_t               index = 0u;
    uint64_t               address = 0u;
    Status                 status = steps_.check("device", session.open_device());
    if (status == Status::ok)
        status = steps_.check("allocation", session.allocate(descriptor.bytes(), descriptor.size(), index));
    if (status == Status::ok) status = steps_.check("paging_queue", session.open_paging_queue());
    if (status == Status::ok) steps_.check("map", session.map(index, descriptor.pages(), address));
    steps_.check_teardown(session);
}

}  // namespace av
