#include "guest/suite/submission_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

namespace {

constexpr uint64_t command_buffer_bytes = 4096u;
constexpr uint64_t command_bytes = 64u;

}

SubmissionScenario::SubmissionScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "submission")
    {
        AV_SCOPE("SubmissionScenario::SubmissionScenario");
    }

void SubmissionScenario::run()
{
    AV_SCOPE("SubmissionScenario::run");
    if (steps_.check("adapter", search_.found() ? Status::ok : Status::not_found) != Status::ok) return;
    GuestSession           session(kmt_, search_.adapter());
    const ClientDescriptor descriptor(command_buffer_bytes);
    uint32_t               index = 0u;
    uint64_t               address = 0u;
    Status                 status = steps_.check("device", session.open_device());
    if (status == Status::ok)
        status = steps_.check("allocation", session.allocate(descriptor.bytes(), descriptor.size(), index));
    if (status == Status::ok) status = steps_.check("paging_queue", session.open_paging_queue());
    if (status == Status::ok) status = steps_.check("map", session.map(index, descriptor.pages(), address));
    if (status == Status::ok) status = steps_.check("context", session.open_context());
    if (status == Status::ok) status = steps_.check("submit", session.submit(address, command_bytes));
    if (status == Status::ok) steps_.check("wait", session.wait());
    steps_.check_teardown(session);
}

}
