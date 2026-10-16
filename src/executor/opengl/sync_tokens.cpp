#include "executor/opengl/sync_tokens.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

SyncTokens::SyncTokens() : tokens_{}, fences_{}, count_(0u), newest_(0u), stand_in_(nullptr), retired_()
{
    AV_SCOPE("SyncTokens::SyncTokens");
}

bool SyncTokens::bind(uint64_t token, void *fence)
{
    AV_SCOPE("SyncTokens::bind");
    /* A null fence is glFenceSync having failed, and a token of zero - at or
     * below every newest - the guest failing to mint one. */
    if (fence == nullptr || token <= newest_ || full()) return false;
    tokens_[count_] = token;
    fences_[count_] = fence;
    count_++;
    newest_ = token;
    return true;
}

bool SyncTokens::full() const
{
    AV_SCOPE("SyncTokens::full");
    return count_ == max_live;
}

void *SyncTokens::oldest() const
{
    AV_SCOPE("SyncTokens::oldest");
    return count_ == 0u ? nullptr : fences_[0];
}

void *SyncTokens::retire_oldest()
{
    AV_SCOPE("SyncTokens::retire_oldest");
    if (count_ == 0u) return nullptr;
    void *const fence = fences_[0];
    /* Older than every live token and newer than every retired one. */
    retired_.add(tokens_[0]);
    drop(0u);
    if (stand_in_ != nullptr) return fence;
    stand_in_ = fence;
    return nullptr;
}

void *SyncTokens::fence_for(uint64_t token) const
{
    AV_SCOPE("SyncTokens::fence_for");
    for (uint32_t i = 0u; i < count_; i++) {
        if (tokens_[i] == token) return fences_[i];
    }
    return retired_.contains(token) ? stand_in_ : nullptr;
}

void *SyncTokens::forget(uint64_t token)
{
    AV_SCOPE("SyncTokens::forget");
    for (uint32_t i = 0u; i < count_; i++) {
        if (tokens_[i] != token) continue;
        void *const fence = fences_[i];
        drop(i);
        return fence;
    }
    retired_.remove(token);
    return nullptr;
}

bool SyncTokens::take_any(void *&fence)
{
    AV_SCOPE("SyncTokens::take_any");
    if (count_ != 0u) {
        count_--;
        fence = fences_[count_];
        tokens_[count_] = 0u;
        fences_[count_] = nullptr;
        return true;
    }
    fence = stand_in_;
    stand_in_ = nullptr;
    retired_.clear();
    if (fence != nullptr) return true;
    newest_ = 0u;
    return false;
}

uint32_t SyncTokens::count() const
{
    AV_SCOPE("SyncTokens::count");
    return count_;
}

void SyncTokens::drop(uint32_t index)
{
    AV_SCOPE("SyncTokens::drop");
    for (uint32_t i = index + 1u; i < count_; i++) {
        tokens_[i - 1u] = tokens_[i];
        fences_[i - 1u] = fences_[i];
    }
    count_--;
    tokens_[count_] = 0u;
    fences_[count_] = nullptr;
}

bool SyncTokens::invariants_hold() const
{
    AV_SCOPE("SyncTokens::invariants_hold");
    if (count_ > max_live || !retired_.invariants_hold()) return false;
    /* A retired token answers through the stand-in, so there is one. */
    if (retired_.runs() != 0u && stand_in_ == nullptr) return false;
    /* Live tokens ascend from above every retired one to no further than the
     * newest bound, each with a sync of its own: a slot sharing the stand-in
     * would be deleted twice. */
    uint64_t below = retired_.newest();
    for (uint32_t i = 0u; i < count_; i++) {
        if (tokens_[i] <= below || fences_[i] == nullptr || fences_[i] == stand_in_) return false;
        below = tokens_[i];
    }
    return below <= newest_;
}

}  // namespace av
