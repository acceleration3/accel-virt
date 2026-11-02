#include "guest/suite/allocation_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

namespace {

constexpr uint64_t allocation_bytes = 65536u;

}

AllocationScenario::AllocationScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "allocation")
    {
        AV_SCOPE("AllocationScenario::AllocationScenario");
    }

void AllocationScenario::run()
{
    AV_SCOPE("AllocationScenario::run");
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession     session(kmt_, search_.adapter());
    ClientDescriptor descriptor(allocation_bytes);
    uint32_t         index = 0u;
    Status           status = steps_.check("device", session.open_device());
    if (status == Status::ok)
        status = steps_.check("create", session.allocate(descriptor.bytes(), descriptor.size(), index));
    descriptor.corrupt_magic();
    if (status == Status::ok)
        steps_.check_refused("refuse_hostile", session.allocate(descriptor.bytes(), descriptor.size(), index));
    steps_.check_teardown(session);
}

}
