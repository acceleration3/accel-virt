#include "harness/executor/retired_sync_tokens_probe.hpp"

namespace av {

RetiredSyncTokensProbe::RetiredSyncTokensProbe(RetiredSyncTokens &target) : retired(target) {}

void RetiredSyncTokensProbe::set_run(uint32_t index, uint64_t first, uint64_t last)
{
    retired.runs_.at(index).first_ = first;
    retired.runs_.at(index).last_ = last;
}

}
