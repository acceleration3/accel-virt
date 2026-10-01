/* avgl.dll's entry points (contracts/icd_interface.md): the fifteen Drv*
 * exports opengl32 looks up by name, each one line into the library in this
 * translation unit's storage - the OpenGL ICD's process-lifetime state
 * (constitution III) - and a DLL entry that does nothing at attach or detach,
 * where the loader lock allows nothing. No C runtime runs constructors, so the
 * storage is constinit and the library constructs itself on the first call.
 *
 * _purecall and _fltused are what the MSVC ABI references with no C runtime to
 * define them, as for avvk.dll (avvk_exports.cpp). */
#include "core/trace/function_scope.hpp"
#include "um/opengl/gl_icd_library.hpp"

#include <windows.h>

/* DrvDescribePixelFormat fills the descriptor as bytes (PixelFormatTable). */
static_assert(sizeof(PIXELFORMATDESCRIPTOR) == 40u);

static long WINAPI avgl_crashed(void *pointers);

static constinit av::GlIcdLibrary avgl_library{&avgl_crashed};

/* The process's unhandled-exception filter while avgl traces it. */
static long WINAPI avgl_crashed(void *pointers)
{
    AV_SCOPE("avgl_crashed");
    return avgl_library.crashed(pointers);
}

extern "C" BOOL WINAPI avgl_entry(HINSTANCE, DWORD, LPVOID) { return TRUE; }

extern "C" BOOL APIENTRY DrvValidateVersion(ULONG version) { return avgl_library.validate_version(version); }

extern "C" void APIENTRY DrvSetCallbackProcs(INT count, PROC *procs)
{ avgl_library.set_callback_procs(count, reinterpret_cast<void *const *>(procs)); }

extern "C" LONG APIENTRY DrvDescribePixelFormat(HDC dc, LONG format, ULONG bytes, PIXELFORMATDESCRIPTOR *pfd)
{ return avgl_library.describe_pixel_format(dc, format, bytes, reinterpret_cast<uint8_t *>(pfd)); }

extern "C" BOOL APIENTRY DrvSetPixelFormat(HDC dc, LONG format) { return avgl_library.set_pixel_format(dc, format); }

extern "C" HANDLE APIENTRY DrvCreateContext(HDC dc)
{ return reinterpret_cast<HANDLE>(avgl_library.create_context(dc)); }

extern "C" HANDLE APIENTRY DrvCreateLayerContext(HDC dc, INT layer)
{ return reinterpret_cast<HANDLE>(avgl_library.create_layer_context(dc, layer)); }

extern "C" BOOL APIENTRY DrvDeleteContext(HANDLE context)
{ return avgl_library.delete_context(reinterpret_cast<uintptr_t>(context)); }

extern "C" void *APIENTRY DrvSetContext(HDC dc, HANDLE context, void *set_proc_table)
{ return avgl_library.set_context(dc, reinterpret_cast<uintptr_t>(context), set_proc_table); }

extern "C" BOOL APIENTRY DrvReleaseContext(HANDLE context)
{ return avgl_library.release_context(reinterpret_cast<uintptr_t>(context)); }

extern "C" BOOL APIENTRY DrvCopyContext(HANDLE source, HANDLE destination, UINT mask)
{ return avgl_library.copy_context(reinterpret_cast<uintptr_t>(source), reinterpret_cast<uintptr_t>(destination), mask); }

extern "C" BOOL APIENTRY DrvShareLists(HANDLE first, HANDLE second)
{ return avgl_library.share_lists(reinterpret_cast<uintptr_t>(first), reinterpret_cast<uintptr_t>(second)); }

extern "C" PROC APIENTRY DrvGetProcAddress(LPCSTR name)
{ return reinterpret_cast<PROC>(avgl_library.proc_address(name)); }

extern "C" BOOL APIENTRY DrvDescribeLayerPlane(HDC dc, INT format, INT plane, UINT bytes, void *descriptor)
{ return avgl_library.describe_layer_plane(dc, format, plane, bytes, descriptor); }

extern "C" BOOL APIENTRY DrvSwapLayerBuffers(HDC dc, UINT planes) { return avgl_library.swap_layer_buffers(dc, planes); }

extern "C" BOOL APIENTRY DrvSwapBuffers(HDC dc) { return avgl_library.swap_buffers(dc); }

extern "C" int _purecall() { ExitProcess(STATUS_ASSERTION_FAILURE); }

extern "C" [[gnu::used]] int _fltused = 0;
