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

WindowsServiceThread::WindowsServiceThread() : loop_(nullptr), thread_(nullptr), stopping_(0)
{
    AV_SCOPE("WindowsServiceThread::WindowsServiceThread");
}

WindowsServiceThread::~WindowsServiceThread()
{
    AV_SCOPE("WindowsServiceThread::~WindowsServiceThread");
    stop();
}

Status WindowsServiceThread::start(icd::ServiceLoop &loop)
{
    AV_SCOPE("WindowsServiceThread::start");
    if (thread_ != nullptr) return Status::invalid_state;
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
    /* Nothing here makes a blocked turn return; the owner has already done
     * whatever does that before calling this. */
    static_cast<void>(WaitForSingleObject(thread_, INFINITE));
    static_cast<void>(CloseHandle(thread_));
    thread_ = nullptr;
    loop_ = nullptr;
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
