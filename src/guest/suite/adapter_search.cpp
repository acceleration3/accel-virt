#include "guest/suite/adapter_search.hpp"

#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"

namespace av {

AdapterSearch::AdapterSearch(Kmt &kmt) : kmt_(kmt), adapter_(0u) {}

/* Nothing is left to report to once the suite is gone. */
AdapterSearch::~AdapterSearch() { static_cast<void>(close()); }

Status AdapterSearch::find(const PciIdentity &identity)
{
    if (adapter_ != 0u) return Status::invalid_state;
    uint32_t     adapters[max_adapters] = {};
    uint32_t     count = 0u;
    const Status enumerated = kmt_.enumerate_adapters(adapters, max_adapters, count);
    if (enumerated != Status::ok) return enumerated;
    const uint32_t held = count < max_adapters ? count : max_adapters;
    for (uint32_t index = 0u; index < held; index++) keep_or_close(adapters[index], identity);
    return adapter_ != 0u ? Status::ok : Status::not_found;
}

void AdapterSearch::keep_or_close(uint32_t adapter, const PciIdentity &identity)
{
    if (adapter == 0u) return;
    PciIdentity reported;
    if (adapter_ == 0u && kmt_.query_identity(adapter, reported) == Status::ok && reported.same_as(identity)) {
        adapter_ = adapter;
        return;
    }
    /* An adapter the suite does not test: a failed close leaves nothing to undo. */
    static_cast<void>(kmt_.close_adapter(adapter));
}

Status AdapterSearch::close()
{
    const uint32_t adapter = adapter_;
    adapter_ = 0u;
    return adapter != 0u ? kmt_.close_adapter(adapter) : Status::ok;
}

bool     AdapterSearch::found() const   { return adapter_ != 0u; }
uint32_t AdapterSearch::adapter() const { return adapter_; }

}  // namespace av
