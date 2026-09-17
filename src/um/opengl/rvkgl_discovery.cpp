/* rvkgl.dll's entry points in its discovery build (T007): the Drv* exports
 * opengl32 looks up by name (contracts/icd_interface.md), each one line into
 * the discovery object in this translation unit's storage, and a DLL entry that
 * does nothing, where the loader lock allows nothing. Replaced by
 * rvkgl_exports.cpp in T030. */
#include "core/trace/function_scope.hpp"
#include "um/opengl/gl_discovery.hpp"

#include <windows.h>

using Entry = av::GlDiscovery::Entry;

static long WINAPI rvkgl_crashed(void *pointers);

static constinit av::GlDiscovery rvkgl_discovery{&rvkgl_crashed};

/* The process's unhandled-exception filter while rvkgl traces it. */
static long WINAPI rvkgl_crashed(void *pointers)
{
    AV_SCOPE("rvkgl_crashed");
    return rvkgl_discovery.crashed(pointers);
}

static uint64_t rvkgl_argument(const void *value)
{
    AV_SCOPE("rvkgl_argument");
    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value));
}

extern "C" BOOL WINAPI rvkgl_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" BOOL APIENTRY DrvValidateVersion(ULONG version) { return rvkgl_discovery.validate(version); }

extern "C" void APIENTRY DrvSetCallbackProcs(INT count, PROC *)
{ static_cast<void>(rvkgl_discovery.answer(Entry::set_callback_procs, static_cast<uint64_t>(count))); }

extern "C" LONG APIENTRY DrvDescribePixelFormat(HDC dc, LONG format, ULONG bytes, PIXELFORMATDESCRIPTOR *pfd)
{ return rvkgl_discovery.describe(rvkgl_argument(dc), format, bytes, pfd); }

extern "C" BOOL APIENTRY DrvSetPixelFormat(HDC dc, LONG)
{ return rvkgl_discovery.answer(Entry::set_pixel_format, rvkgl_argument(dc)); }

extern "C" HANDLE APIENTRY DrvCreateContext(HDC dc)
{ return rvkgl_discovery.decline(Entry::create_context, rvkgl_argument(dc)); }

extern "C" HANDLE APIENTRY DrvCreateLayerContext(HDC dc, INT)
{ return rvkgl_discovery.decline(Entry::create_layer_context, rvkgl_argument(dc)); }

extern "C" BOOL APIENTRY DrvDeleteContext(HANDLE context)
{ return rvkgl_discovery.answer(Entry::delete_context, rvkgl_argument(context)); }

extern "C" void *APIENTRY DrvSetContext(HDC dc, HANDLE, void *)
{ return rvkgl_discovery.decline(Entry::set_context, rvkgl_argument(dc)); }

extern "C" BOOL APIENTRY DrvReleaseContext(HANDLE context)
{ return rvkgl_discovery.answer(Entry::release_context, rvkgl_argument(context)); }

extern "C" BOOL APIENTRY DrvCopyContext(HANDLE source, HANDLE, UINT)
{ return rvkgl_discovery.answer(Entry::copy_context, rvkgl_argument(source)); }

extern "C" BOOL APIENTRY DrvShareLists(HANDLE first, HANDLE)
{ return rvkgl_discovery.answer(Entry::share_lists, rvkgl_argument(first)); }

extern "C" PROC APIENTRY DrvGetProcAddress(LPCSTR name)
{ return reinterpret_cast<PROC>(rvkgl_discovery.look_up(name)); }

extern "C" BOOL APIENTRY DrvDescribeLayerPlane(HDC dc, INT, INT, UINT, void *)
{ return rvkgl_discovery.answer(Entry::describe_layer_plane, rvkgl_argument(dc)); }

extern "C" BOOL APIENTRY DrvSwapLayerBuffers(HDC dc, UINT)
{ return rvkgl_discovery.answer(Entry::swap_layer_buffers, rvkgl_argument(dc)); }

extern "C" BOOL APIENTRY DrvSwapBuffers(HDC dc) { return rvkgl_discovery.answer(Entry::swap_buffers, rvkgl_argument(dc)); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" int _fltused = 0;
