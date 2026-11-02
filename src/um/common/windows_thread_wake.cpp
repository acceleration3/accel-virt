#include "um/common/windows_thread_wake.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

WindowsThreadWake::WindowsThreadWake() : event_(nullptr)
{
    AV_SCOPE("WindowsThreadWake::WindowsThreadWake");
}

WindowsThreadWake::~WindowsThreadWake()
{
    AV_SCOPE("WindowsThreadWake::~WindowsThreadWake");
    if (event_ != nullptr) static_cast<void>(CloseHandle(event_));
}

Status WindowsThreadWake::initialize()
{
    AV_SCOPE("WindowsThreadWake::initialize");
    if (event_ != nullptr) return Status::ok;
    event_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    return event_ != nullptr ? Status::ok : Status::no_memory;
}

void WindowsThreadWake::raise()
{
    AV_SCOPE("WindowsThreadWake::raise");
    if (event_ != nullptr) static_cast<void>(SetEvent(event_));
}

bool WindowsThreadWake::wait(uint32_t milliseconds)
{
    AV_SCOPE("WindowsThreadWake::wait");
    return event_ != nullptr && WaitForSingleObject(event_, milliseconds) == WAIT_OBJECT_0;
}

}
