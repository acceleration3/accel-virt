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
 * Another context's tokens falling between two of this one's split a run, and
 * the runs grow with them. Only when no room can be had for another are the
 * two oldest joined, which counts the tokens between them as retired here
 * too: the price is that a token older than every run still kept apart can
 * answer signaled on a context that never bound it - another context's sync
 * from that long ago, or one deleted. Never a token newer than this context's
 * last retirement, and never a pointer: what answers for a retired token is a
 * sync of this context's own. */
#ifndef AV_RETIRED_SYNC_TOKENS_HPP
#define AV_RETIRED_SYNC_TOKENS_HPP

#include "core/support/growable_array.hpp"
#include "core/support/status.hpp"

#include <stdint.h>

namespace av {

class RetiredSyncTokens {
public:
    explicit RetiredSyncTokens(BlockSource &blocks);
    RetiredSyncTokens(const RetiredSyncTokens &) = delete;
    RetiredSyncTokens &operator=(const RetiredSyncTokens &) = delete;

    /* Holds `token`, which is newer than every token held: one that is not,
     * or zero, is not a retirement any context makes, and is not added.
     * no_memory only when there is no room for a first run at all. */
    [[nodiscard]] Status add(uint64_t token);
    bool                 contains(uint64_t token) const;
    /* Lets go of `token`, splitting the run it is inside. With no room for the
     * second half the token stays: a deleted sync answering signaled is seen
     * only by a guest that uses one after deleting it. Nothing for a token not
     * held. */
    void                 remove(uint64_t token);
    /* The newest token held; zero for none. */
    uint64_t             newest() const;
    void                 clear();
    uint32_t             runs() const;

    bool invariants_hold() const;

private:
    friend struct RetiredSyncTokensProbe;

    struct Run {
        uint64_t first_;
        uint64_t last_;
    };

    /* The run `token` is inside, or runs() for none. */
    uint32_t run_of(uint64_t token) const;
    void     join_oldest();

    GrowableArray<Run> runs_;
};

}  // namespace av

#endif  // AV_RETIRED_SYNC_TOKENS_HPP
