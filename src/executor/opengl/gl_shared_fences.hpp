#ifndef AV_GL_SHARED_FENCES_HPP
#define AV_GL_SHARED_FENCES_HPP

#include "device/shim/posix_lock.hpp"

#include <stdint.h>

namespace av {

class GlSharedFences {
public:
    static constexpr uint32_t most_fences = 512u;

    GlSharedFences();
    GlSharedFences(const GlSharedFences &) = delete;
    GlSharedFences &operator=(const GlSharedFences &) = delete;

    void  publish(uint64_t token, void *fence);
    void  withdraw(void *fence);
    void *find(uint64_t token);

    uint32_t count();

    bool invariants_hold();

private:
    struct Entry {
        uint64_t token_;
        void    *fence_;
    };

    PosixLock lock_;
    Entry     entries_[most_fences];
    uint32_t  next_;
    uint32_t  count_;
};

}

#endif
