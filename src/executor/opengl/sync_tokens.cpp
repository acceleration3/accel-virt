#include "executor/opengl/sync_tokens.hpp"

#include "core/support/sorted_search.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

SyncTokens::SyncTokens(BlockSource &blocks)
    : live_(blocks, Pool::paged), newest_(0u), stand_in_(nullptr), retired_(blocks)
{
    AV_SCOPE("SyncTokens::SyncTokens");
}

Status SyncTokens::bind(uint64_t token, void *fence)
{
    AV_SCOPE("SyncTokens::bind");
    /* A null fence is glFenceSync having failed, and a token of zero - at or
     * below every newest - the guest failing to mint one. */
    if (fence == nullptr || token <= newest_) return Status::invalid_parameter;
    const Status held = live_.append(Live{token, fence});
    if (held != Status::ok) return held;
    newest_ = token;
    return Status::ok;
}

bool SyncTokens::crowded() const
{
    AV_SCOPE("SyncTokens::crowded");
    return live_.size() >= retire_after;
}

void *SyncTokens::oldest() const
{
    AV_SCOPE("SyncTokens::oldest");
    return live_.size() == 0u ? nullptr : live_.at(0u).fence_;
}

Status SyncTokens::retire_oldest(void *&deleted)
{
    AV_SCOPE("SyncTokens::retire_oldest");
    deleted = nullptr;
    if (live_.size() == 0u) return Status::ok;
    /* Older than every live token and newer than every retired one. */
    const Status kept = retired_.add(live_.at(0u).token_);
    if (kept != Status::ok) return kept;
    void *const fence = live_.at(0u).fence_;
    live_.remove_at(0u);
    if (stand_in_ == nullptr) stand_in_ = fence;
    else deleted = fence;
    return Status::ok;
}

uint32_t SyncTokens::index_of(uint64_t token) const
{
    AV_SCOPE("SyncTokens::index_of");
    return SortedSearch<Live, &Live::token_>(live_).exact(token);
}

void *SyncTokens::fence_for(uint64_t token) const
{
    AV_SCOPE("SyncTokens::fence_for");
    const uint32_t index = index_of(token);
    if (index != live_.size()) return live_.at(index).fence_;
    return retired_.contains(token) ? stand_in_ : nullptr;
}

void *SyncTokens::forget(uint64_t token)
{
    AV_SCOPE("SyncTokens::forget");
    const uint32_t index = index_of(token);
    if (index == live_.size()) {
        retired_.remove(token);
        return nullptr;
    }
    void *const fence = live_.at(index).fence_;
    live_.remove_at(index);
    return fence;
}

bool SyncTokens::take_any(void *&fence)
{
    AV_SCOPE("SyncTokens::take_any");
    if (live_.size() != 0u) {
        fence = live_.at(live_.size() - 1u).fence_;
        live_.truncate(live_.size() - 1u);
        return true;
    }
    fence = stand_in_;
    stand_in_ = nullptr;
    retired_.clear();
    live_.clear();
    if (fence != nullptr) return true;
    newest_ = 0u;
    return false;
}

uint32_t SyncTokens::count() const
{
    AV_SCOPE("SyncTokens::count");
    return live_.size();
}

bool SyncTokens::invariants_hold() const
{
    AV_SCOPE("SyncTokens::invariants_hold");
    if (!retired_.invariants_hold()) return false;
    /* A retired token answers through the stand-in, so there is one. */
    if (retired_.runs() != 0u && stand_in_ == nullptr) return false;
    /* Live tokens ascend from above every retired one to no further than the
     * newest bound, each with a sync of its own: a slot sharing the stand-in
     * would be deleted twice. */
    uint64_t below = retired_.newest();
    for (const Live &live : live_) {
        if (live.token_ <= below || live.fence_ == nullptr || live.fence_ == stand_in_) return false;
        below = live.token_;
    }
    return below <= newest_;
}

}  // namespace av
