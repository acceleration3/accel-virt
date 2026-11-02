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
    void   pause(bool driven) override;
    void   wake() override;
    uint64_t wake_handle() const override;
    uint64_t woken() const override;
    void     await_woken(uint64_t seen, uint64_t timeout) override;

    void run();
    void relay();

private:
    void wait(HANDLE timer, DWORD milliseconds);
    void stop_relay();

    icd::ServiceLoop *loop_;
    HANDLE            thread_;
    HANDLE            woken_;
    HANDLE            ticker_;
    HANDLE            driver_wake_;
    HANDLE            relay_thread_;
    HANDLE            relay_quit_;
    mutable SRWLOCK   control_;
    uint64_t           woken_count_;
    mutable SRWLOCK    woken_lock_;
    CONDITION_VARIABLE woken_moved_;
    volatile LONG     stopping_;
};

}

#endif
