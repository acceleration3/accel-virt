#include "harness/executor/retired_sync_tokens_probe.hpp"

namespace av {

RetiredSyncTokensProbe::RetiredSyncTokensProbe(RetiredSyncTokens &target) : retired(target) {}

void RetiredSyncTokensProbe::set_count(uint32_t count) { retired.count_ = count; }

void RetiredSyncTokensProbe::set_run(uint32_t index, uint64_t first, uint64_t last)
{
    retired.firsts_[index] = first;
    retired.lasts_[index] = last;
}

}  // namespace av
