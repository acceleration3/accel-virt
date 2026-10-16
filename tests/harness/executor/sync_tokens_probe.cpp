#include "harness/executor/sync_tokens_probe.hpp"

namespace av {

SyncTokensProbe::SyncTokensProbe(SyncTokens &target) : tokens(target) {}

void SyncTokensProbe::set_count(uint32_t count) { tokens.count_ = count; }

void SyncTokensProbe::set_token(uint32_t index, uint64_t token) { tokens.tokens_[index] = token; }

void SyncTokensProbe::set_fence(uint32_t index, void *fence) { tokens.fences_[index] = fence; }

void SyncTokensProbe::set_newest(uint64_t newest) { tokens.newest_ = newest; }

void SyncTokensProbe::set_stand_in(void *fence) { tokens.stand_in_ = fence; }

RetiredSyncTokens &SyncTokensProbe::retired() { return tokens.retired_; }

}  // namespace av
