#include "harness/executor/sync_tokens_probe.hpp"

namespace av {

SyncTokensProbe::SyncTokensProbe(SyncTokens &target) : tokens(target) {}

void SyncTokensProbe::set_token(uint32_t index, uint64_t token) { tokens.live_.at(index).token_ = token; }

void SyncTokensProbe::set_fence(uint32_t index, void *fence) { tokens.live_.at(index).fence_ = fence; }

void SyncTokensProbe::set_newest(uint64_t newest) { tokens.newest_ = newest; }

void SyncTokensProbe::set_stand_in(void *fence) { tokens.stand_in_ = fence; }

RetiredSyncTokens &SyncTokensProbe::retired() { return tokens.retired_; }

}  // namespace av
