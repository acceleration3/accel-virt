/* A device's call lock: one runtime call into a device at a time, from any
 * thread - a free-threaded or multithreaded device is called from several at
 * once - while a call that comes back into the same device on its own thread
 * (the runtime calling the driver from inside the driver's callback) holds it
 * already rather than waiting on itself. */
#ifndef AV_D3D_REENTRANT_CALL_LOCK_HPP
#define AV_D3D_REENTRANT_CALL_LOCK_HPP

#include <stdint.h>

namespace av {
class ThreadLock;
namespace icd {
class CallingThread;
}
}  // namespace av

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
    /* The thread holding it, 0 for none; read without the lock, by a thread
     * asking whether it is the one. */
    uint32_t owner_;
    uint32_t depth_;
};

}  // namespace av::d3d

#endif  // AV_D3D_REENTRANT_CALL_LOCK_HPP
