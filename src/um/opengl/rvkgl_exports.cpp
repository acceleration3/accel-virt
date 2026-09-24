/* rvkgl.dll's entry points (contracts/icd_interface.md): the fifteen Drv*
 * exports opengl32 looks up by name, each one line into the library in this
 * translation unit's storage - the OpenGL ICD's process-lifetime state
 * (constitution III) - and a DLL entry that does nothing at attach or detach,
 * where the loader lock allows nothing. No C runtime runs constructors, so the
 * storage is constinit and the library constructs itself on the first call.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * define them, as for rvkvk.dll (rvkvk_exports.cpp). */
#include "core/trace/function_scope.hpp"
#include "um/opengl/gl_icd_library.hpp"

#include <windows.h>

/* DrvDescribePixelFormat fills the descriptor as bytes (PixelFormatTable). */
static_assert(sizeof(PIXELFORMATDESCRIPTOR) == 40u);

static long WINAPI rvkgl_crashed(void *pointers);

static constinit av::GlIcdLibrary rvkgl_library{&rvkgl_crashed};

/* The process's unhandled-exception filter while rvkgl traces it. */
static long WINAPI rvkgl_crashed(void *pointers)
{
    AV_SCOPE("rvkgl_crashed");
    return rvkgl_library.crashed(pointers);
}

extern "C" BOOL WINAPI rvkgl_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" BOOL APIENTRY DrvValidateVersion(ULONG version) { return rvkgl_library.validate_version(version); }

extern "C" void APIENTRY DrvSetCallbackProcs(INT count, PROC *procs)
{ rvkgl_library.set_callback_procs(count, reinterpret_cast<void *const *>(procs)); }

extern "C" LONG APIENTRY DrvDescribePixelFormat(HDC dc, LONG format, ULONG bytes, PIXELFORMATDESCRIPTOR *pfd)
{ return rvkgl_library.describe_pixel_format(dc, format, bytes, reinterpret_cast<uint8_t *>(pfd)); }

extern "C" BOOL APIENTRY DrvSetPixelFormat(HDC dc, LONG format) { return rvkgl_library.set_pixel_format(dc, format); }

extern "C" HANDLE APIENTRY DrvCreateContext(HDC dc)
{ return reinterpret_cast<HANDLE>(rvkgl_library.create_context(dc)); }

extern "C" HANDLE APIENTRY DrvCreateLayerContext(HDC dc, INT layer)
{ return reinterpret_cast<HANDLE>(rvkgl_library.create_layer_context(dc, layer)); }

extern "C" BOOL APIENTRY DrvDeleteContext(HANDLE context)
{ return rvkgl_library.delete_context(reinterpret_cast<uintptr_t>(context)); }

extern "C" void *APIENTRY DrvSetContext(HDC dc, HANDLE context, void *set_proc_table)
{ return rvkgl_library.set_context(dc, reinterpret_cast<uintptr_t>(context), set_proc_table); }

extern "C" BOOL APIENTRY DrvReleaseContext(HANDLE context)
{ return rvkgl_library.release_context(reinterpret_cast<uintptr_t>(context)); }

extern "C" BOOL APIENTRY DrvCopyContext(HANDLE source, HANDLE destination, UINT mask)
{ return rvkgl_library.copy_context(reinterpret_cast<uintptr_t>(source), reinterpret_cast<uintptr_t>(destination), mask); }

extern "C" BOOL APIENTRY DrvShareLists(HANDLE first, HANDLE second)
{ return rvkgl_library.share_lists(reinterpret_cast<uintptr_t>(first), reinterpret_cast<uintptr_t>(second)); }

extern "C" PROC APIENTRY DrvGetProcAddress(LPCSTR name)
{ return reinterpret_cast<PROC>(rvkgl_library.proc_address(name)); }

extern "C" BOOL APIENTRY DrvDescribeLayerPlane(HDC dc, INT format, INT plane, UINT bytes, void *descriptor)
{ return rvkgl_library.describe_layer_plane(dc, format, plane, bytes, descriptor); }

extern "C" BOOL APIENTRY DrvSwapLayerBuffers(HDC dc, UINT planes) { return rvkgl_library.swap_layer_buffers(dc, planes); }

extern "C" BOOL APIENTRY DrvSwapBuffers(HDC dc) { return rvkgl_library.swap_buffers(dc); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
