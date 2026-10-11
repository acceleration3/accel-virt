#include "um/common/windows_service_thread.hpp"

#include "core/trace/function_scope.hpp"

namespace av {

/* The thread procedure. An ABI boundary C requires: CreateThread takes a bare
 * function and one argument, so the object is the argument and this forwards
 * straight into it. */
extern "C" DWORD WINAPI av_service_thread_run(LPVOID service)
{
    static_cast<WindowsServiceThread *>(service)->run();
    return 0u;
}

WindowsServiceThread::WindowsServiceThread()
    : loop_(nullptr), thread_(nullptr), woken_(nullptr), ticker_(nullptr), stopping_(0)
{
    AV_SCOPE("WindowsServiceThread::WindowsServiceThread");
}

WindowsServiceThread::~WindowsServiceThread()
{
    AV_SCOPE("WindowsServiceThread::~WindowsServiceThread");
    stop();
    if (woken_ != nullptr) static_cast<void>(CloseHandle(woken_));
    if (ticker_ != nullptr) static_cast<void>(CloseHandle(ticker_));
}

Status WindowsServiceThread::start(icd::ServiceLoop &loop)
{
    AV_SCOPE("WindowsServiceThread::start");
    if (thread_ != nullptr) return Status::invalid_state;
    if (woken_ == nullptr) woken_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (woken_ == nullptr) return Status::no_memory;
    if (ticker_ == nullptr)
        ticker_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    loop_ = &loop;
    InterlockedExchange(&stopping_, 0);
    thread_ = CreateThread(nullptr, 0u, &av_service_thread_run, this, 0u, nullptr);
    if (thread_ == nullptr) {
        loop_ = nullptr;
        return Status::no_memory;
    }
    return Status::ok;
}

void WindowsServiceThread::stop()
{
    AV_SCOPE("WindowsServiceThread::stop");
    if (thread_ == nullptr) return;
    InterlockedExchange(&stopping_, 1);
    /* A resting loop returns now; nothing else here makes a blocked turn
     * return, which the owner has already seen to before calling this. */
    if (woken_ != nullptr) static_cast<void>(SetEvent(woken_));
    static_cast<void>(WaitForSingleObject(thread_, INFINITE));
    static_cast<void>(CloseHandle(thread_));
    thread_ = nullptr;
    loop_ = nullptr;
}

void WindowsServiceThread::rest()
{
    AV_SCOPE("WindowsServiceThread::rest");
    /* A moment at most: a wake that came between the loop finding nothing and
     * resting is an event already set, and anything else is found next turn. */
    constexpr DWORD moment_ms = 100u;
    if (woken_ != nullptr) static_cast<void>(WaitForSingleObject(woken_, moment_ms));
}

void WindowsServiceThread::pause()
{
    AV_SCOPE("WindowsServiceThread::pause");
    constexpr DWORD millisecond = 1u;
    if (woken_ == nullptr) return;
    /* Relative, in 100 ns units. */
    LARGE_INTEGER due = {};
    due.QuadPart = -10000;
    if (ticker_ != nullptr && SetWaitableTimerEx(ticker_, &due, 0, nullptr, nullptr, nullptr, 0u) != FALSE) {
        const HANDLE either[2] = {woken_, ticker_};
        static_cast<void>(WaitForMultipleObjects(2u, either, FALSE, INFINITE));
        return;
    }
    static_cast<void>(WaitForSingleObject(woken_, millisecond));
}

void WindowsServiceThread::wake()
{
    AV_SCOPE("WindowsServiceThread::wake");
    if (woken_ != nullptr) static_cast<void>(SetEvent(woken_));
}

bool WindowsServiceThread::running() const
{
    AV_SCOPE("WindowsServiceThread::running");
    return thread_ != nullptr;
}

void WindowsServiceThread::run()
{
    AV_SCOPE("WindowsServiceThread::run");
    while (InterlockedCompareExchange(&stopping_, 0, 0) == 0) {
        if (!loop_->turn()) break;
    }
}

}  // namespace av
