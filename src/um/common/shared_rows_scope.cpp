#include "um/common/shared_rows_scope.hpp"

#include "core/trace/function_scope.hpp"

#include <windows.h>

extern "C" BOOL WINAPI K32EnumProcessModules(HANDLE process, HMODULE *modules, DWORD bytes, LPDWORD needed);

namespace av {

namespace {

constexpr uint32_t max_modules = 512u;

#if defined(_WIN64)
constexpr wchar_t umd_name[] = L"avumd.dll";
#else
constexpr wchar_t umd_name[] = L"avumd32.dll";
#endif

}

SharedRowsScope::SharedRowsScope() : withdraw_(), asked_(0u)
{
    AV_SCOPE("SharedRowsScope::SharedRowsScope");
    HMODULE loaded[max_modules] = {};
    DWORD   needed = 0u;
    uint32_t count = 0u;
    if (K32EnumProcessModules(GetCurrentProcess(), loaded, static_cast<DWORD>(sizeof loaded), &needed))
        count = static_cast<uint32_t>(needed / sizeof(HMODULE));
    if (count > max_modules) count = max_modules;
    for (uint32_t at = 0u; at < count; ++at) ask(loaded[at]);
    ask(GetModuleHandleW(umd_name));
}

void SharedRowsScope::ask(void *module)
{
    AV_SCOPE("SharedRowsScope::ask");
    if (module == nullptr || asked_ == max_asked) return;
    HMODULE const handle = static_cast<HMODULE>(module);
    const auto    asking =
        reinterpret_cast<void (*)()>(reinterpret_cast<void *>(GetProcAddress(handle, "AvAskSharedRows")));
    const auto withdraw =
        reinterpret_cast<Withdraw>(reinterpret_cast<void *>(GetProcAddress(handle, "AvWithdrawSharedRows")));
    if (asking == nullptr || withdraw == nullptr) return;
    for (uint32_t at = 0u; at < asked_; ++at)
        if (withdraw_[at] == withdraw) return;
    asking();
    withdraw_[asked_++] = withdraw;
}

SharedRowsScope::~SharedRowsScope()
{
    AV_SCOPE("SharedRowsScope::~SharedRowsScope");
    for (uint32_t at = 0u; at < asked_; ++at) withdraw_[at]();
}

}
