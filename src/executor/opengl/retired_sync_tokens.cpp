#include "executor/opengl/retired_sync_tokens.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

RetiredSyncTokens::RetiredSyncTokens() : firsts_{}, lasts_{}, count_(0u)
{
    AV_SCOPE("RetiredSyncTokens::RetiredSyncTokens");
}

void RetiredSyncTokens::add(uint64_t token)
{
    AV_SCOPE("RetiredSyncTokens::add");
    if (token <= newest()) return;
    if (count_ != 0u && token - 1u == lasts_[count_ - 1u]) {
        lasts_[count_ - 1u] = token;
        return;
    }
    if (count_ == max_runs) join_oldest();
    firsts_[count_] = token;
    lasts_[count_] = token;
    count_++;
}

bool RetiredSyncTokens::contains(uint64_t token) const
{
    AV_SCOPE("RetiredSyncTokens::contains");
    return run_of(token) != count_;
}

void RetiredSyncTokens::remove(uint64_t token)
{
    AV_SCOPE("RetiredSyncTokens::remove");
    const uint32_t index = run_of(token);
    if (index == count_) return;
    if (firsts_[index] == lasts_[index]) {
        close_run(index);
        return;
    }
    if (token == firsts_[index]) {
        firsts_[index]++;
        return;
    }
    if (token == lasts_[index]) {
        lasts_[index]--;
        return;
    }
    if (count_ < max_runs) split_run(index, token);
}

uint64_t RetiredSyncTokens::newest() const
{
    AV_SCOPE("RetiredSyncTokens::newest");
    return count_ == 0u ? 0u : lasts_[count_ - 1u];
}

void RetiredSyncTokens::clear()
{
    AV_SCOPE("RetiredSyncTokens::clear");
    count_ = 0u;
}

uint32_t RetiredSyncTokens::runs() const
{
    AV_SCOPE("RetiredSyncTokens::runs");
    return count_;
}

uint32_t RetiredSyncTokens::run_of(uint64_t token) const
{
    AV_SCOPE("RetiredSyncTokens::run_of");
    for (uint32_t i = 0u; i < count_; i++) {
        if (token >= firsts_[i] && token <= lasts_[i]) return i;
    }
    return count_;
}

void RetiredSyncTokens::close_run(uint32_t index)
{
    AV_SCOPE("RetiredSyncTokens::close_run");
    for (uint32_t i = index + 1u; i < count_; i++) {
        firsts_[i - 1u] = firsts_[i];
        lasts_[i - 1u] = lasts_[i];
    }
    count_--;
}

/* `token` is strictly inside the run, so both halves keep at least one. */
void RetiredSyncTokens::split_run(uint32_t index, uint64_t token)
{
    AV_SCOPE("RetiredSyncTokens::split_run");
    for (uint32_t i = count_; i > index + 1u; i--) {
        firsts_[i] = firsts_[i - 1u];
        lasts_[i] = lasts_[i - 1u];
    }
    firsts_[index + 1u] = token + 1u;
    lasts_[index + 1u] = lasts_[index];
    lasts_[index] = token - 1u;
    count_++;
}

/* The oldest tokens are the likeliest to be signaled wherever they were
 * made, so the gap given up is the oldest one. */
void RetiredSyncTokens::join_oldest()
{
    AV_SCOPE("RetiredSyncTokens::join_oldest");
    lasts_[0] = lasts_[1];
    close_run(1u);
}

bool RetiredSyncTokens::invariants_hold() const
{
    AV_SCOPE("RetiredSyncTokens::invariants_hold");
    if (count_ > max_runs) return false;
    /* Ascending, never zero, and a token apart: runs that touched would have
     * been one. */
    uint64_t after = 0u;
    for (uint32_t i = 0u; i < count_; i++) {
        if (firsts_[i] <= after || lasts_[i] < firsts_[i]) return false;
        if (lasts_[i] == ~uint64_t{0}) return i + 1u == count_;
        after = lasts_[i] + 1u;
    }
    return true;
}

}  // namespace av
