#ifndef AV_WINDOWS_THREAD_WAKE_HPP
#define AV_WINDOWS_THREAD_WAKE_HPP

#include "core/support/status.hpp"
#include "icd/common/thread_wake.hpp"

#include <windows.h>

namespace av {

class WindowsThreadWake final : public icd::ThreadWake {
public:
    WindowsThreadWake();
    ~WindowsThreadWake();

    [[nodiscard]] Status initialize();
    void                 raise() override;
    bool                 wait(uint32_t milliseconds) override;

private:
    HANDLE event_;
};

}

#endif
