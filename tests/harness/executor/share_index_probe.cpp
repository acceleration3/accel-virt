#include "harness/executor/share_index_probe.hpp"

namespace av {

ShareIndexProbe::ShareIndexProbe(const ShareIndex &target) : index(target) {}

uint32_t ShareIndexProbe::buckets(uint32_t domain) const
{
    ShareIndex::DomainIds *const *const table = index.domains_.find(domain);
    return table != nullptr ? (*table)->ids().buckets() : 0u;
}

uint32_t ShareIndexProbe::domains() const { return index.domains_.count(); }

}
