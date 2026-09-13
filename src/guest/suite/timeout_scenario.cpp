#include "guest/suite/timeout_scenario.hpp"

#include "guest/suite/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

namespace {

constexpr uint64_t command_buffer_bytes = 4096u;
constexpr uint64_t command_bytes = 64u;

}  // namespace

TimeoutScenario::TimeoutScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "timeout") {}

void TimeoutScenario::run()
{
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession held(kmt_, search_.adapter());
    GuestSession other(kmt_, search_.adapter());
    uint64_t     held_address = 0u;
    uint64_t     other_address = 0u;
    /* The held context first: the run aims the host's stall at the first
     * context it is asked to make. */
    if (steps_.check("held_set_up", prepare(held, held_address)) != Status::ok) return;
    if (steps_.check("other_set_up", prepare(other, other_address)) != Status::ok) return;
    if (steps_.check("held_submit", held.submit(held_address, command_bytes)) != Status::ok) return;
    if (steps_.check("other_submit", other.submit(other_address, command_bytes)) != Status::ok) return;
    /* The held wait outlives the timeout: dxgkrnl resets the node under it and
     * the wait comes back refused. */
    const Status held_wait = held.wait();
    const Status other_wait = other.wait();
    steps_.check_refused("held_lost", held_wait);
    steps_.check("other_completed", other_wait);
    steps_.check_teardown(other);
    steps_.check_teardown(held);
}

Status TimeoutScenario::prepare(GuestSession &session, uint64_t &address)
{
    const ClientDescriptor descriptor(command_buffer_bytes);
    uint32_t               index = 0u;
    Status                 status = session.open_device();
    if (status == Status::ok) status = session.allocate(descriptor.bytes(), descriptor.size(), index);
    if (status == Status::ok) status = session.open_paging_queue();
    if (status == Status::ok) status = session.map(index, descriptor.pages(), address);
    if (status == Status::ok) status = session.open_context();
    return status;
}

}  // namespace av
