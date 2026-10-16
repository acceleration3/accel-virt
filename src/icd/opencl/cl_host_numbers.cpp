#include "icd/opencl/cl_host_numbers.hpp"

#include "core/trace/function_scope.hpp"

namespace av::clicd {

ClHostNumbers::ClHostNumbers(BlockSource &memory) : inline_{}, spilled_(memory, Pool::paged)
{
    AV_SCOPE("ClHostNumbers::ClHostNumbers");
}

uint64_t *ClHostNumbers::room(uint32_t count)
{
    AV_SCOPE("ClHostNumbers::room");
    if (count <= kept_inline) return inline_;
    if (spilled_.resize(count, 0u) != Status::ok) return nullptr;
    return spilled_.begin();
}

bool ClHostNumbers::invariants_hold() const
{
    AV_SCOPE("ClHostNumbers::invariants_hold");
    return spilled_.invariants_hold() && (spilled_.size() == 0u || spilled_.size() > kept_inline);
}

}  // namespace av::clicd
