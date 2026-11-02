#include "um/common/windows_calling_thread.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

uint32_t WindowsCallingThread::thread_id()
{
    AV_SCOPE("WindowsCallingThread::thread_id");
    return GetCurrentThreadId();
}

}
