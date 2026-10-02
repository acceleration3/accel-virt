/* icd::WakeSignal over an auto-reset Windows event. Translation only. */
#ifndef AV_WINDOWS_WAKE_SIGNAL_HPP
#define AV_WINDOWS_WAKE_SIGNAL_HPP

#include "core/support/status.hpp"
#include "icd/opengl/wake_signal.hpp"

#include <windows.h>

namespace av {

class WindowsWakeSignal final : public icd::WakeSignal {
public:
    WindowsWakeSignal();
    ~WindowsWakeSignal();

    /* no_memory when Windows has no event to give. */
    [[nodiscard]] Status initialize();
    void                 raise() override;
    bool                 wait(uint32_t milliseconds) override;

private:
    HANDLE event_;
};

}  // namespace av

#endif  // AV_WINDOWS_WAKE_SIGNAL_HPP
