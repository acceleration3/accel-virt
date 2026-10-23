#include "executor/opengl/retired_sync_tokens.hpp"

#include "core/support/sorted_search.hpp"
#include "core/trace/function_scope.hpp"

namespace av {

RetiredSyncTokens::RetiredSyncTokens(BlockSource &blocks) : runs_(blocks, Pool::paged)
{
    AV_SCOPE("RetiredSyncTokens::RetiredSyncTokens");
}

Status RetiredSyncTokens::add(uint64_t token)
{
    AV_SCOPE("RetiredSyncTokens::add");
    if (token <= newest()) return Status::ok;
    const uint32_t count = runs_.size();
    if (count != 0u && token - 1u == runs_.at(count - 1u).last_) {
        runs_.at(count - 1u).last_ = token;
        return Status::ok;
    }
    if (runs_.append(Run{token, token}) == Status::ok) return Status::ok;
    if (count < 2u) return Status::no_memory;
    join_oldest();
    return runs_.append(Run{token, token});
}

bool RetiredSyncTokens::contains(uint64_t token) const
{
    AV_SCOPE("RetiredSyncTokens::contains");
    return run_of(token) != runs_.size();
}

void RetiredSyncTokens::remove(uint64_t token)
{
    AV_SCOPE("RetiredSyncTokens::remove");
    const uint32_t index = run_of(token);
    if (index == runs_.size()) return;
    Run &run = runs_.at(index);
    if (run.first_ == run.last_) {
        runs_.remove_at(index);
        return;
    }
    if (token == run.first_) {
        run.first_++;
        return;
    }
    if (token == run.last_) {
        run.last_--;
        return;
    }
    /* `token` is strictly inside the run, so both halves keep at least one. */
    const Run after{token + 1u, run.last_};
    if (runs_.insert_at(index + 1u, after) == Status::ok) runs_.at(index).last_ = token - 1u;
}

uint64_t RetiredSyncTokens::newest() const
{
    AV_SCOPE("RetiredSyncTokens::newest");
    return runs_.size() == 0u ? 0u : runs_.at(runs_.size() - 1u).last_;
}

void RetiredSyncTokens::clear()
{
    AV_SCOPE("RetiredSyncTokens::clear");
    runs_.clear();
}

uint32_t RetiredSyncTokens::runs() const
{
    AV_SCOPE("RetiredSyncTokens::runs");
    return runs_.size();
}

/* Runs ascend and never touch, so the last run starting at or below `token`
 * is the only one that can hold it. */
uint32_t RetiredSyncTokens::run_of(uint64_t token) const
{
    AV_SCOPE("RetiredSyncTokens::run_of");
    const uint32_t row = SortedSearch<Run, &Run::first_>(runs_).at_or_below(token);
    return row != runs_.size() && token <= runs_.at(row).last_ ? row : runs_.size();
}

/* The oldest tokens are the likeliest to be signaled wherever they were
 * made, so the gap given up is the oldest one. */
void RetiredSyncTokens::join_oldest()
{
    AV_SCOPE("RetiredSyncTokens::join_oldest");
    runs_.at(0u).last_ = runs_.at(1u).last_;
    runs_.remove_at(1u);
}

bool RetiredSyncTokens::invariants_hold() const
{
    AV_SCOPE("RetiredSyncTokens::invariants_hold");
    /* Ascending, never zero, and a token apart: runs that touched would have
     * been one. */
    uint64_t after = 0u;
    for (uint32_t i = 0u; i < runs_.size(); i++) {
        const Run &run = runs_.at(i);
        if (run.first_ <= after || run.last_ < run.first_) return false;
        if (run.last_ == ~uint64_t{0}) return i + 1u == runs_.size();
        after = run.last_ + 1u;
    }
    return true;
}

}  // namespace av
