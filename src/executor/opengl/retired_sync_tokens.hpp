/* The tokens of one OpenGL context's GLsyncs whose driver object the host let
 * go of once it saw them signaled, held as runs of consecutive tokens.
 *
 * A signaled sync never changes again until it is deleted, so answering for
 * one needs no driver object, only the knowledge that its token was bound here
 * and not deleted since. An application that never calls glDeleteSync -
 * DaVinci Resolve fences every frame and deletes nothing - would otherwise
 * hold a driver object per fence for as long as the context lives. Tokens are
 * minted in increasing order and retired in the order they were bound, so a
 * context that fences alone keeps one run however many syncs it leaves.
 *
 * Another context's tokens falling between two of this one's split a run.
 * Past max_runs the two oldest runs are joined, which counts the tokens
 * between them as retired here too: the bound's price is that a token older
 * than every run still kept apart can answer signaled on a context that never
 * bound it - another context's sync from that long ago, or one deleted. Never
 * a token newer than this context's last retirement, and never a pointer:
 * what answers for a retired token is a sync of this context's own. */
#ifndef AV_RETIRED_SYNC_TOKENS_HPP
#define AV_RETIRED_SYNC_TOKENS_HPP

#include <stdint.h>

namespace av {

class RetiredSyncTokens {
public:
    /* Each a pair of tokens. A context fencing alone needs one; this is room
     * for that many other contexts' fences interleaved before old runs join. */
    static constexpr uint32_t max_runs = 64u;

    RetiredSyncTokens();
    RetiredSyncTokens(const RetiredSyncTokens &) = delete;
    RetiredSyncTokens &operator=(const RetiredSyncTokens &) = delete;

    /* Holds `token`, which is newer than every token held: one that is not,
     * or zero, is not a retirement any context makes, and is not added. */
    void     add(uint64_t token);
    bool     contains(uint64_t token) const;
    /* Lets go of `token`, splitting the run it is inside. With every run in
     * use the token stays: a deleted sync answering signaled is seen only by
     * a guest that uses one after deleting it. Nothing for a token not held. */
    void     remove(uint64_t token);
    /* The newest token held; zero for none. */
    uint64_t newest() const;
    void     clear();
    uint32_t runs() const;

    bool invariants_hold() const;

private:
    friend struct RetiredSyncTokensProbe;

    /* The run `token` is inside, or `count_` for none. */
    uint32_t run_of(uint64_t token) const;
    void     close_run(uint32_t index);
    void     split_run(uint32_t index, uint64_t token);
    void     join_oldest();

    uint64_t firsts_[max_runs];
    uint64_t lasts_[max_runs];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_RETIRED_SYNC_TOKENS_HPP
