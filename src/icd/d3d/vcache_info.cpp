#include "icd/d3d/vcache_info.hpp"

#include "core/trace/function_scope.hpp"

#include <string.h>

namespace av::d3d {

VcacheInfo::VcacheInfo() : pattern_(pattern_cach), optimization_(0u), cache_size_(0u), magic_(0u)
{
    AV_SCOPE("VcacheInfo::VcacheInfo");
}

bool VcacheInfo::write(void *to, uint64_t room) const
{
    AV_SCOPE("VcacheInfo::write");
    if (to == nullptr || room < bytes) return false;
    const uint32_t words[bytes / sizeof(uint32_t)] = {pattern_, optimization_, cache_size_, magic_};
    memcpy(to, words, bytes);
    return true;
}

}
