/* The host fences one OpenGL context's guest tokens name.
 *
 * A GLsync is a pointer into the driver and cannot cross the wire, so the guest
 * mints a token, the host binds it to whatever glFenceSync made, and every
 * later command that names the fence names the token. A token the host never
 * bound answers nothing, which is what stops a guest handing the driver a
 * pointer it chose (feature 042).
 *
 * A GLsync lives until glDeleteSync, and an application may never call it.
 * So the driver objects held are only the newest max_live: when the table is
 * full its oldest sync, once the GPU has passed it, is retired - its token
 * kept among the RetiredSyncTokens and its driver object deleted. A signaled
 * sync answers every question the same way until it is deleted, so a retired
 * token answers through the stand-in, the first sync this context retired,
 * kept for that. Nothing grows with the fences a guest leaves. The one thing
 * retired syncs then share is a debug label: glObjectPtrLabel on one labels
 * the stand-in. */
#ifndef AV_SYNC_TOKENS_HPP
#define AV_SYNC_TOKENS_HPP

#include "executor/opengl/retired_sync_tokens.hpp"

#include <stdint.h>

namespace av {

class SyncTokens {
public:
    /* Past any real pipeline depth: a sync is retired only once the GPU has
     * passed it, so a full table waits for its oldest. */
    static constexpr uint32_t max_live = 256u;

    SyncTokens();
    SyncTokens(const SyncTokens &) = delete;
    SyncTokens &operator=(const SyncTokens &) = delete;

    /* Binds `token` to `fence`. False for a null fence, a full table, or a
     * token not newer than every one bound before: a guest mints them in
     * increasing order and one context's fences reach the host in the order
     * they were made, so an older one is a reuse - which would lose the fence
     * a live token named and leak the driver's object - or an invention. */
    [[nodiscard]] bool bind(uint64_t token, void *fence);
    bool               full() const;
    /* The driver sync bound longest ago and not yet retired; null for none. */
    void              *oldest() const;
    /* Retires the oldest sync, which the caller has seen signaled. Answers
     * the driver sync to delete, or null: the first retired becomes the
     * stand-in, and there was none to retire. */
    void              *retire_oldest();
    /* The driver sync `token` answers through: its own, the stand-in for a
     * retired token, or null for one this context never bound. */
    void              *fence_for(uint64_t token) const;
    /* Takes `token` out for glDeleteSync, answering the driver sync to delete
     * with it: null for a retired token, whose answer was the stand-in that
     * other tokens still answer through, and for one never bound. */
    void              *forget(uint64_t token);
    /* Every driver sync held, the stand-in last, dropped one at a time: how
     * a closing context deletes each while it still has a context to delete
     * it with. False once none is left, and the table is as new. */
    [[nodiscard]] bool take_any(void *&fence);

    /* The driver syncs held for live tokens, the stand-in not counted. */
    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct SyncTokensProbe;

    /* Drops live slot `index`, keeping the rest in the order they were bound. */
    void drop(uint32_t index);

    /* Live tokens ascending, which is the order they were bound in. */
    uint64_t          tokens_[max_live];
    void             *fences_[max_live];
    uint32_t          count_;
    /* The newest token ever bound; one at or below it is refused. */
    uint64_t          newest_;
    void             *stand_in_;
    RetiredSyncTokens retired_;
};

}  // namespace av

#endif  // AV_SYNC_TOKENS_HPP
