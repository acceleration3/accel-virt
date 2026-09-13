#include "guest/suite/host_visible_scenario.hpp"

#include "guest/suite/adapter_search.hpp"
#include "guest/suite/client_descriptor.hpp"
#include "guest/suite/guest_session.hpp"

namespace av {

HostVisibleScenario::HostVisibleScenario(Kmt &kmt, GuestReport &report, const AdapterSearch &search)
    : kmt_(kmt), search_(search), steps_(kmt, report, "host_visible"), matched_(0u) {}

void HostVisibleScenario::run()
{
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
        write_and_read_back(static_cast<volatile unsigned char *>(view));
        static_cast<void>(steps_.check_measured("pattern", Status::ok, matched_));
        steps_.check("unlock", session.unlock(index));
    }
    steps_.check_teardown(session);
}

void HostVisibleScenario::write_and_read_back(volatile unsigned char *view)
{
    const auto pattern = [](uint32_t offset) { return static_cast<unsigned char>(1u + offset * 7u % 251u); };
    for (uint32_t offset = 0u; offset < pattern_bytes; offset++) view[offset] = pattern(offset);
    matched_ = 0u;
    for (uint32_t offset = 0u; offset < pattern_bytes; offset++)
        matched_ += static_cast<uint32_t>(view[offset] == pattern(offset));
}

uint32_t HostVisibleScenario::matched_bytes() const { return matched_; }

}  // namespace av
