/* icd::ServiceThread over a Windows thread. Translation only.
 *
 * The thread runs the loop until it answers false or `stop` is called. A turn
 * may block - the OpenCL event watcher's does, on D3DKMT - so `stop` sets the
 * flag and then waits, and what makes the blocked turn return is the owner's
 * business: the watcher's owner sends its transport a nop, which the device
 * answers and the wait ends. */
#ifndef AV_WINDOWS_SERVICE_THREAD_HPP
#define AV_WINDOWS_SERVICE_THREAD_HPP

#include "icd/common/service_thread.hpp"

#include <windows.h>

namespace av {

class WindowsServiceThread final : public icd::ServiceThread {
public:
    WindowsServiceThread();
    ~WindowsServiceThread();

    Status start(icd::ServiceLoop &loop) override;
    void   stop() override;
    bool   running() const override;
    void   rest() override;
    void   wake() override;

    /* What the thread runs. Public because the thread procedure C requires is
     * handed this and nothing else. */
    void run();

private:
    icd::ServiceLoop *loop_;
    HANDLE            thread_;
    /* Auto-reset: what a resting loop waits on, set by wake() and stop(). */
    HANDLE            woken_;
    /* Read by the thread, written by whoever stops it. */
    volatile LONG     stopping_;
};

}  // namespace av

#endif  // AV_WINDOWS_SERVICE_THREAD_HPP
