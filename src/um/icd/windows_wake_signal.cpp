#include "um/icd/windows_wake_signal.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WindowsWakeSignal::WindowsWakeSignal() : event_(nullptr)
{
    AV_SCOPE("WindowsWakeSignal::WindowsWakeSignal");
}

WindowsWakeSignal::~WindowsWakeSignal()
{
    AV_SCOPE("WindowsWakeSignal::~WindowsWakeSignal");
    if (event_ != nullptr) static_cast<void>(CloseHandle(event_));
}

Status WindowsWakeSignal::initialize()
{
    AV_SCOPE("WindowsWakeSignal::initialize");
    if (event_ != nullptr) return Status::ok;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return event_ != nullptr ? Status::ok : Status::no_memory;
}

void WindowsWakeSignal::raise()
{
    AV_SCOPE("WindowsWakeSignal::raise");
    if (event_ != nullptr) static_cast<void>(SetEvent(event_));
}

bool WindowsWakeSignal::wait(uint32_t milliseconds)
{
    AV_SCOPE("WindowsWakeSignal::wait");
    return event_ != nullptr && WaitForSingleObject(event_, milliseconds) == WAIT_OBJECT_0;
}

}  // namespace av
