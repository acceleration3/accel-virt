#include "guest/suite/adapter_search.hpp"

#include "core/trace/function_scope.hpp"
#include "guest/kmt/kmt.hpp"
#include "guest/kmt/pci_identity.hpp"

namespace av {

AdapterSearch::AdapterSearch(Kmt &kmt) : kmt_(kmt), adapter_(0u), luid_(0u)
{
    AV_SCOPE("AdapterSearch::AdapterSearch");
}

/* Nothing is left to report to once the suite is gone. */
AdapterSearch::~AdapterSearch()
{
    AV_SCOPE("AdapterSearch::~AdapterSearch");
    static_cast<void>(close());
}

/* With more than one avgpu adapter, the first the kernel enumerates may be
 * a lesser GPU - an integrated one beside the discrete GPU driving the
 * display - so the one the primary display is on is looked for first, as
 * DXGI lists it first and a D3D application gets it by default. */
Status AdapterSearch::find(const PciIdentity &identity)
{
    AV_SCOPE("AdapterSearch::find");
    uint64_t primary = 0u;
    if (kmt_.primary_display_luid(primary) == Status::ok && primary != 0u && find(identity, primary) == Status::ok)
        return Status::ok;
    return find(identity, 0u);
}

Status AdapterSearch::find(const PciIdentity &identity, uint64_t luid)
{
    AV_SCOPE("AdapterSearch::find");
    if (adapter_ != 0u) return Status::invalid_state;
    luid_ = luid;
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
    AV_SCOPE("AdapterSearch::keep_or_close");
    if (adapter == 0u) return;
    PciIdentity reported;
    uint64_t luid = 0u;
    const bool wanted = luid_ == 0u || (kmt_.adapter_luid(adapter, luid) == Status::ok && luid == luid_);
    if (adapter_ == 0u && wanted && kmt_.query_identity(adapter, reported) == Status::ok &&
        reported.same_subsystem_as(identity)) {
        adapter_ = adapter;
        return;
    }
    /* An adapter the suite does not test: a failed close leaves nothing to undo. */
    static_cast<void>(kmt_.close_adapter(adapter));
}

Status AdapterSearch::close()
{
    AV_SCOPE("AdapterSearch::close");
    const uint32_t adapter = adapter_;
    adapter_ = 0u;
    return adapter != 0u ? kmt_.close_adapter(adapter) : Status::ok;
}

bool     AdapterSearch::found() const
{
    AV_SCOPE("AdapterSearch::found");
    return adapter_ != 0u;
}
uint32_t AdapterSearch::adapter() const
{
    AV_SCOPE("AdapterSearch::adapter");
    return adapter_;
}

}  // namespace av
