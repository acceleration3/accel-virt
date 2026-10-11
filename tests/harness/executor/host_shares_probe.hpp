/* Reaches into a HostShares so an invariant test can break it the ways a bug
 * would: a share the index lost, an index entry naming nothing, a holder no
 * share links to - and mend it again. Declared a friend by HostShares;
 * nothing outside the tests uses it. */
#ifndef AV_HOST_SHARES_PROBE_HPP
#define AV_HOST_SHARES_PROBE_HPP

#include "executor/sharing/host_shares.hpp"

#include <stdint.h>

namespace av {

class SessionBackend;
class ShareHolder;

struct HostSharesProbe {
    explicit HostSharesProbe(HostShares &target);
    void         unindex(uint32_t domain, uint64_t id);
    void         reindex(uint32_t domain, uint64_t id);
    void         index_nothing(uint32_t domain, uint64_t id);
    void         forget_nothing(uint32_t domain, uint64_t id);
    ShareHolder *stray_holder(SessionBackend &session);
    void         drop_stray(ShareHolder *holder);

    HostShares &shares;
    Share      *unindexed = nullptr;
};

}  // namespace av

#endif  // AV_HOST_SHARES_PROBE_HPP
