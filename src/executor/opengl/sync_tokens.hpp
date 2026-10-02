/* The host fences one OpenGL context's guest tokens name.
 *
 * A GLsync is a pointer into the driver and cannot cross the wire, so the guest
 * mints a token, the host binds it to whatever glFenceSync made, and every
 * later command that names the fence names the token. A token the host never
 * bound answers nothing, which is what stops a guest handing the driver a
 * pointer it chose (feature 042). */
#ifndef AV_SYNC_TOKENS_HPP
#define AV_SYNC_TOKENS_HPP

#include <stdint.h>

namespace av {

class SyncTokens {
public:
    /* A GLsync lives from glFenceSync to glDeleteSync. This is past any real
     * pipeline depth, and a guest that never deletes has to be bounded
     * somewhere. */
    static constexpr uint32_t max_tokens = 256u;

    SyncTokens();
    SyncTokens(const SyncTokens &) = delete;
    SyncTokens &operator=(const SyncTokens &) = delete;

    /* Binds `token` to `fence`. False when full, or when the token is already
     * bound - a guest reusing a live token would otherwise lose the fence it
     * named and leak the driver's object. */
    [[nodiscard]] bool bind(uint64_t token, void *fence);
    /* The fence `token` names, or null for one nobody bound. */
    void              *fence_for(uint64_t token) const;
    /* Drops a binding. Nothing for a token nobody bound. */
    void               forget(uint64_t token);
    /* Any binding, dropped, with the fence it named: how a closing context
     * deletes each GLsync while it still has a context to delete it with.
     * False once none is left. */
    [[nodiscard]] bool take_any(void *&fence);

    uint32_t count() const;

    bool invariants_hold() const;

private:
    friend struct SyncTokensProbe;

    uint64_t tokens_[max_tokens];
    void    *fences_[max_tokens];
    uint32_t count_;
};

}  // namespace av

#endif  // AV_SYNC_TOKENS_HPP
