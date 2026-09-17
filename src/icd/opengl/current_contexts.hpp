/* Which context each thread has current (research.md §3): the calling
 * thread's is its ThreadSlot value, and each context names the thread it is
 * current on, so a context is current on at most one thread and a thread that
 * asks for another's is refused with that thread's state untouched (US3.2). */
#ifndef AV_GLICD_CURRENT_CONTEXTS_HPP
#define AV_GLICD_CURRENT_CONTEXTS_HPP

#include <stdint.h>

namespace av::glicd {

class GuestGlContext;
class ThreadSlot;

class CurrentContexts {
public:
    explicit CurrentContexts(ThreadSlot &slot);
    CurrentContexts(const CurrentContexts &) = delete;
    CurrentContexts &operator=(const CurrentContexts &) = delete;

    /* false, changing nothing, when `context` is current on another thread;
     * otherwise the calling thread's previous context is released and
     * `context` is current on it. */
    bool            make_current(GuestGlContext &context);
    /* The calling thread has nothing current; nothing when it had nothing. */
    void            release();
    /* Null when the calling thread has nothing current. */
    GuestGlContext *current() const;
    bool            current_elsewhere(const GuestGlContext &context) const;
    /* The contexts current on some thread. */
    uint32_t        anywhere() const;
    uint32_t        thread() const;

    bool invariants_hold() const;

private:
    ThreadSlot &slot_;
    uint32_t    anywhere_;
};

}  // namespace av::glicd

#endif  // AV_GLICD_CURRENT_CONTEXTS_HPP
