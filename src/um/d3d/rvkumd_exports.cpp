/* rvkumd.dll's entry points (contracts/umd_interface.md): the two names the
 * Direct3D runtimes resolve out of the export table - `OpenAdapter10_2` for
 * d3d11.dll and `OpenAdapter12` for D3D12Core.dll - each one line into the
 * library in this translation unit's storage, the D3D user-mode driver's
 * process-lifetime state (constitution III), and a DLL entry that does nothing
 * at attach or detach, where the loader lock allows nothing. No C runtime runs
 * constructors, so the storage is constinit and the library constructs itself
 * on the first call.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * define them, as for rvkvk.dll (rvkvk_exports.cpp). */
#include "core/trace/function_scope.hpp"
#include "um/d3d/umd_library.hpp"

#include <windows.h>

static long WINAPI rvkumd_crashed(void *pointers);

static constinit av::UmdLibrary rvkumd_library{&rvkumd_crashed};

/* The process's unhandled-exception filter while rvkumd traces it. */
static long WINAPI rvkumd_crashed(void *pointers)
{
    AV_SCOPE("rvkumd_crashed");
    return rvkumd_library.crashed(pointers);
}

extern "C" BOOL WINAPI rvkumd_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" HRESULT APIENTRY OpenAdapter10_2(void *args) { return rvkumd_library.open_adapter10_2(args); }

extern "C" HRESULT APIENTRY OpenAdapter12(void *args) { return rvkumd_library.open_adapter12(args); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
