#include "harness/executor/host_shares_probe.hpp"

namespace av {

HostSharesProbe::HostSharesProbe(HostShares &target) : shares(target) {}

void HostSharesProbe::unindex(uint32_t domain, uint64_t id)
{
    unindexed = shares.index_.find(domain, id);
    shares.index_.remove(domain, id);
}

void HostSharesProbe::reindex(uint32_t domain, uint64_t id)
{
    static_cast<void>(shares.index_.insert(domain, id, unindexed));
}

void HostSharesProbe::index_nothing(uint32_t domain, uint64_t id)
{
    static unsigned char nothing;
    static_cast<void>(shares.index_.insert(domain, id, reinterpret_cast<Share *>(&nothing)));
}

void HostSharesProbe::forget_nothing(uint32_t domain, uint64_t id) { shares.index_.remove(domain, id); }

ShareHolder *HostSharesProbe::stray_holder(SessionBackend &session)
{
    ShareHolder *holder = nullptr;
    static_cast<void>(shares.holders_.create(holder, session, 0u, 0u));
    return holder;
}

void HostSharesProbe::drop_stray(ShareHolder *holder) { static_cast<void>(shares.holders_.destroy(holder)); }

}  // namespace av
