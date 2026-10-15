#include "um/common/shared_rows_scope.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

namespace av {

namespace {

/* The Direct3D user-mode driver as a process of this bitness loads it
 * (avgpu.inf's UserModeDriverName and its Wow value). */
#if defined(_WIN64)
constexpr wchar_t umd_name[] = L"avumd.dll";
#else
constexpr wchar_t umd_name[] = L"avumd32.dll";
#endif

}  // namespace

SharedRowsScope::SharedRowsScope() : withdraw_(nullptr)
{
    AV_SCOPE("SharedRowsScope::SharedRowsScope");
    HMODULE const umd = GetModuleHandleW(umd_name);
    if (umd == nullptr) return;
    const auto ask = reinterpret_cast<void (*)()>(reinterpret_cast<void *>(GetProcAddress(umd, "AvAskSharedRows")));
    const auto withdraw =
        reinterpret_cast<void (*)()>(reinterpret_cast<void *>(GetProcAddress(umd, "AvWithdrawSharedRows")));
    if (ask == nullptr || withdraw == nullptr) return;
    ask();
    withdraw_ = withdraw;
}

SharedRowsScope::~SharedRowsScope()
{
    AV_SCOPE("SharedRowsScope::~SharedRowsScope");
    if (withdraw_ != nullptr) withdraw_();
}

}  // namespace av
