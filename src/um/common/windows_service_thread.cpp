#include "um/common/windows_service_thread.hpp"

#include "core/trace/function_scope.hpp"

namespace av {
extern "C" DWORD WINAPI av_service_thread_run(LPVOID service)
{
    static_cast<WindowsServiceThread *>(service)->run();
    return 0u;
}

extern "C" DWORD WINAPI av_service_relay_run(LPVOID service)
{
    static_cast<WindowsServiceThread *>(service)->relay();
    return 0u;
}

WindowsServiceThread::WindowsServiceThread()
    : loop_(nullptr), thread_(nullptr), woken_(nullptr), ticker_(nullptr),
      driver_wake_(CreateEventW(nullptr, FALSE, FALSE, nullptr)), relay_thread_(nullptr), relay_quit_(nullptr),
      control_(SRWLOCK_INIT), woken_count_(0u),
      woken_lock_(SRWLOCK_INIT), woken_moved_(CONDITION_VARIABLE_INIT), stopping_(0)
{
    AV_SCOPE("WindowsServiceThread::WindowsServiceThread");
}

WindowsServiceThread::~WindowsServiceThread()
{
    AV_SCOPE("WindowsServiceThread::~WindowsServiceThread");
    stop();
    if (woken_ != nullptr) static_cast<void>(CloseHandle(woken_));
    if (ticker_ != nullptr) static_cast<void>(CloseHandle(ticker_));
    if (relay_quit_ != nullptr) static_cast<void>(CloseHandle(relay_quit_));
    if (driver_wake_ != nullptr) static_cast<void>(CloseHandle(driver_wake_));
}

Status WindowsServiceThread::start(icd::ServiceLoop &loop)
{
    AV_SCOPE("WindowsServiceThread::start");
    AcquireSRWLockExclusive(&control_);
    Status started = Status::ok;
    if (thread_ != nullptr) started = Status::invalid_state;
    if (started == Status::ok && woken_ == nullptr) woken_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (started == Status::ok && woken_ == nullptr) started = Status::no_memory;
    if (started == Status::ok) {
        if (ticker_ == nullptr)
            ticker_ = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                             TIMER_ALL_ACCESS);
        if (driver_wake_ != nullptr && relay_quit_ == nullptr) relay_quit_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (driver_wake_ != nullptr && relay_quit_ != nullptr && ResetEvent(relay_quit_) != FALSE)
            relay_thread_ = CreateThread(nullptr, 0u, &av_service_relay_run, this, 0u, nullptr);
        loop_ = &loop;
        InterlockedExchange(&stopping_, 0);
        thread_ = CreateThread(nullptr, 0u, &av_service_thread_run, this, 0u, nullptr);
        if (thread_ == nullptr) {
            loop_ = nullptr;
            stop_relay();
            started = Status::no_memory;
        }
    }
    ReleaseSRWLockExclusive(&control_);
    return started;
}

void WindowsServiceThread::stop()
{
    AV_SCOPE("WindowsServiceThread::stop");
    AcquireSRWLockExclusive(&control_);
    if (thread_ != nullptr) {
        InterlockedExchange(&stopping_, 1);
        if (woken_ != nullptr) static_cast<void>(SetEvent(woken_));
        static_cast<void>(WaitForSingleObject(thread_, INFINITE));
        static_cast<void>(CloseHandle(thread_));
        thread_ = nullptr;
        loop_ = nullptr;
    }
    stop_relay();
    ReleaseSRWLockExclusive(&control_);
}

void WindowsServiceThread::stop_relay()
{
    AV_SCOPE("WindowsServiceThread::stop_relay");
    if (relay_thread_ == nullptr) return;
    static_cast<void>(SetEvent(relay_quit_));
    static_cast<void>(WaitForSingleObject(relay_thread_, INFINITE));
    static_cast<void>(CloseHandle(relay_thread_));
    relay_thread_ = nullptr;
}

void WindowsServiceThread::relay()
{
    AV_SCOPE("WindowsServiceThread::relay");
    const HANDLE either[2] = {relay_quit_, driver_wake_};
    while (WaitForMultipleObjects(2u, either, FALSE, INFINITE) == WAIT_OBJECT_0 + 1u) {
        AcquireSRWLockExclusive(&woken_lock_);
        ++woken_count_;
        ReleaseSRWLockExclusive(&woken_lock_);
        WakeAllConditionVariable(&woken_moved_);
        static_cast<void>(SetEvent(woken_));
    }
}

void WindowsServiceThread::wait(HANDLE timer, DWORD milliseconds)
{
    AV_SCOPE("WindowsServiceThread::wait");
    const HANDLE either[2] = {woken_, timer};
    static_cast<void>(WaitForMultipleObjects(timer != nullptr ? 2u : 1u, either, FALSE, milliseconds));
}

void WindowsServiceThread::rest()
{
    AV_SCOPE("WindowsServiceThread::rest");
    constexpr DWORD moment_ms = 100u;
    if (woken_ != nullptr) wait(nullptr, moment_ms);
}

void WindowsServiceThread::pause(bool driven)
{
    AV_SCOPE("WindowsServiceThread::pause");
    if (woken_ == nullptr) return;
    if (driven && relay_thread_ != nullptr) {
        wait(nullptr, INFINITE);
        return;
    }
    LARGE_INTEGER due = {};
    due.QuadPart = -10000;
    if (ticker_ != nullptr && SetWaitableTimerEx(ticker_, &due, 0, nullptr, nullptr, nullptr, 0u) != FALSE) {
        wait(ticker_, INFINITE);
        return;
    }
    constexpr DWORD millisecond = 1u;
    wait(nullptr, millisecond);
}

void WindowsServiceThread::wake()
{
    AV_SCOPE("WindowsServiceThread::wake");
    if (woken_ != nullptr) static_cast<void>(SetEvent(woken_));
}

uint64_t WindowsServiceThread::wake_handle() const
{
    AV_SCOPE("WindowsServiceThread::wake_handle");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(driver_wake_));
}

uint64_t WindowsServiceThread::woken() const
{
    AV_SCOPE("WindowsServiceThread::woken");
    AcquireSRWLockShared(&woken_lock_);
    const uint64_t count = woken_count_;
    ReleaseSRWLockShared(&woken_lock_);
    return count;
}

void WindowsServiceThread::await_woken(uint64_t seen, uint64_t timeout)
{
    AV_SCOPE("WindowsServiceThread::await_woken");
    constexpr uint64_t ns_per_ms = 1000000u;
    const uint64_t     budget_ms = timeout / ns_per_ms + (timeout % ns_per_ms != 0u ? 1u : 0u);
    const ULONGLONG    started = GetTickCount64();
    AcquireSRWLockShared(&woken_lock_);
    while (woken_count_ == seen) {
        const uint64_t spent = GetTickCount64() - started;
        if (spent >= budget_ms) break;
        const uint64_t left = budget_ms - spent;
        const DWORD    sleep_ms = left >= INFINITE ? INFINITE - 1u : static_cast<DWORD>(left);
        if (SleepConditionVariableSRW(&woken_moved_, &woken_lock_, sleep_ms, CONDITION_VARIABLE_LOCKMODE_SHARED) ==
                FALSE &&
            GetLastError() == ERROR_TIMEOUT)
            break;
    }
    ReleaseSRWLockShared(&woken_lock_);
}

bool WindowsServiceThread::running() const
{
    AV_SCOPE("WindowsServiceThread::running");
    AcquireSRWLockShared(&control_);
    const bool started = thread_ != nullptr;
    ReleaseSRWLockShared(&control_);
    return started;
}

void WindowsServiceThread::run()
{
    AV_SCOPE("WindowsServiceThread::run");
    while (InterlockedCompareExchange(&stopping_, 0, 0) == 0) {
        if (!loop_->turn()) break;
    }
}

}
