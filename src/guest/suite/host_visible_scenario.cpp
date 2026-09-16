#include "guest/suite/host_visible_scenario.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/suite/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
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
    const ClientDescriptor descriptor(pattern_bytes);
    uint32_t               index = 0u;
    void                  *view = nullptr;
    Status                 status = steps_.check("device", session.open_device());
    if (status == Status::ok)
        status = steps_.check("allocation", session.allocate(descriptor.bytes(), descriptor.size(), index));
    if (status == Status::ok) status = steps_.check("lock", session.lock(index, view));
    if (status == Status::ok) {
        auto *const memory = static_cast<volatile unsigned char *>(view);
        pattern_.write(memory);
        matched_ = pattern_.matched(memory);
        static_cast<void>(steps_.check_measured("pattern", Status::ok, matched_));
        steps_.check("unlock", session.unlock(index));
    }
    steps_.check_teardown(session);
}

uint32_t HostVisibleScenario::matched_bytes() const
{
    AV_SCOPE("HostVisibleScenario::matched_bytes");
    return matched_;
}

}  // namespace av
