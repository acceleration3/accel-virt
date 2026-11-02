#include "core/trace/function_scope.hpp"
#include "um/d3d/umd_library.hpp"

#include <windows.h>

static long WINAPI avumd_crashed(void *pointers);

static constinit av::UmdLibrary avumd_library{&avumd_crashed};

static long WINAPI avumd_crashed(void *pointers)
{
    AV_SCOPE("avumd_crashed");
    return avumd_library.crashed(pointers);
}

extern "C" BOOL WINAPI avumd_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" HRESULT APIENTRY OpenAdapter10_2(void *args) { return avumd_library.open_adapter10_2(args); }

extern "C" HRESULT APIENTRY OpenAdapter12(void *args) { return avumd_library.open_adapter12(args); }

extern "C" HRESULT APIENTRY OpenAdapter(void *args) { return avumd_library.open_adapter9(args); }

extern "C" void APIENTRY AvAskSharedRows() { avumd_library.ask_shared_rows(); }

extern "C" void APIENTRY AvWithdrawSharedRows() { avumd_library.withdraw_shared_rows(); }
