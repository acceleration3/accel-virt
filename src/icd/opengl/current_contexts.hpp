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

    bool            make_current(GuestGlContext &context);
    void            release();
    GuestGlContext *current() const;
    bool            current_elsewhere(const GuestGlContext &context) const;
    uint32_t        anywhere() const;
    uint32_t        thread() const;

    bool invariants_hold() const;

private:
    ThreadSlot &slot_;
    uint32_t    anywhere_;
};

}

#endif
