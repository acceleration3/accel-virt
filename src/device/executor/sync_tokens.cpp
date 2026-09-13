#include "device/executor/sync_tokens.hpp"

namespace av {

SyncTokens::SyncTokens() : tokens_{}, fences_{}, count_(0u) {}

bool SyncTokens::bind(uint64_t token, void *fence)
{
    /* A token of zero is the guest failing to mint one, and a null fence is
     * glFenceSync having failed; neither is a binding worth keeping. */
    if (token == 0u || fence == nullptr) return false;
    if (fence_for(token) != nullptr) return false;
    if (count_ >= max_tokens) return false;
    tokens_[count_] = token;
    fences_[count_] = fence;
    count_++;
    return true;
}

void *SyncTokens::fence_for(uint64_t token) const
{
    if (token == 0u) return nullptr;
    for (uint32_t i = 0u; i < count_; i++) {
        if (tokens_[i] == token) return fences_[i];
    }
    return nullptr;
}

void SyncTokens::forget(uint64_t token)
{
    for (uint32_t i = 0u; i < count_; i++) {
        if (tokens_[i] != token) continue;
        tokens_[i] = tokens_[count_ - 1u];
        fences_[i] = fences_[count_ - 1u];
        count_--;
        return;
    }
}

void SyncTokens::forget_all() { count_ = 0u; }

uint32_t SyncTokens::count() const { return count_; }

bool SyncTokens::invariants_hold() const
{
    if (count_ > max_tokens) return false;
    for (uint32_t i = 0u; i < count_; i++) {
        if (tokens_[i] == 0u || fences_[i] == nullptr) return false;
    }
    return true;
}

}  // namespace av
