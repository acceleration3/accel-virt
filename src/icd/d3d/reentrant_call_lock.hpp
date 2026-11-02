#ifndef AV_D3D_REENTRANT_CALL_LOCK_HPP
#define AV_D3D_REENTRANT_CALL_LOCK_HPP

#include <stdint.h>

namespace av {
class ThreadLock;
namespace icd {
class CallingThread;
}
}

namespace av::d3d {

class ReentrantCallLock {
public:
    ReentrantCallLock(ThreadLock &lock, icd::CallingThread &caller);
    ReentrantCallLock(const ReentrantCallLock &) = delete;
    ReentrantCallLock &operator=(const ReentrantCallLock &) = delete;

    void acquire();
    void release();
    bool invariants_hold() const;

private:
    ThreadLock &lock_;
    icd::CallingThread &caller_;
    uint32_t owner_;
    uint32_t depth_;
};

}

#endif
