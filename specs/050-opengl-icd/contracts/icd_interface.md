# Contract: how the system opengl32.dll reaches `rvkgl.dll`

Source of truth for opengl32's side: `opengl32.dll` 10.0.26100.5074, pinned in
`tools/re/binaries.txt` (research.md §1). This is what the INF and the DLL
promise it.

## Registration (INF)

In the software key of every rvkgpu adapter (`[Rvkgpu_SoftwareSettings]`):

| Value | Type | Data |
|---|---|---|
| `OpenGLDriverName` | `REG_MULTI_SZ` (`0x00010000`) | `%13%\rvkgl.dll` |
| `OpenGLVersion` | `REG_DWORD` (`0x00010001`) | 2 |
| `OpenGLFlags` | `REG_DWORD` (`0x00010001`) | 1 |

No `...Wow` value (061), nothing under `HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\OpenGLDrivers`.
`rvkgl.dll` is copied to DIRID 13 and covered by the package catalog.

## Exports

Exactly these, undecorated, from `rvkgl.def`; each is one line into the root.

| Export | Behaviour |
|---|---|
| `DrvValidateVersion(ULONG)` | TRUE for 2, else FALSE |
| `DrvSetCallbackProcs(INT, PROC*)` | keeps up to 9 procs; ignores the rest |
| `DrvDescribePixelFormat(HDC, INT, ULONG, PIXELFORMATDESCRIPTOR*)` | the table's count; fills the descriptor for an index in range and a size ≥ 40; 0 for a DC with no window |
| `DrvSetPixelFormat(HDC, LONG)` | TRUE once per window for an index in range; FALSE for a window with a format, a memory DC, an index out of range |
| `DrvCreateContext(HDC)` | a `DHGLRC` for a DC whose window has a format; NULL otherwise |
| `DrvCreateLayerContext(HDC, INT)` | as `DrvCreateContext` for layer 0; NULL for any other |
| `DrvDeleteContext(DHGLRC)` | TRUE; a context current on another thread is refused; the host context is destroyed once |
| `DrvSetContext(HDC, DHGLRC, PFN_SETPROCTABLE)` | makes the host context on first use; the `GLCLTPROCTABLE` (336 entries) on success; NULL with `ERROR_BUSY` when current on another thread, `ERROR_INVALID_HANDLE` for a handle this ICD did not make, `ERROR_INVALID_PIXEL_FORMAT` for a DC whose window's format differs from the context's |
| `DrvReleaseContext(DHGLRC)` | TRUE for the calling thread's current context, else FALSE |
| `DrvCopyContext(DHGLRC, DHGLRC, UINT)` | FALSE (not supported; `ERROR_NOT_SUPPORTED`) |
| `DrvShareLists(DHGLRC, DHGLRC)` | TRUE while neither context has been made current; FALSE after |
| `DrvGetProcAddress(LPCSTR)` | the by-name row: GL entries beyond the table, the WGL extension functions, the probe's fault hook; NULL otherwise |
| `DrvDescribeLayerPlane(...)` | FALSE (no layer planes) |
| `DrvSwapLayerBuffers(HDC, UINT)` | `DrvSwapBuffers` for `WGL_SWAP_MAIN_PLANE`; FALSE otherwise |
| `DrvSwapBuffers(HDC)` | resolve, read back, blit; TRUE, or FALSE with nothing current on the DC's window |

## The proc table

`GLCLTPROCTABLE { int cEntries = 336; PROC glDispatchTable[336]; }` in the order
`tools/codegen/profiles/opengl_proc_table.txt` records (Microsoft's
`glCltRGBAProcTable`). Every entry is non-null: a generated wrapper, an ICD
entry, or the ICD's `GL_INVALID_OPERATION` recorder for a GL 1.1 command the
core profile lacks.

## WGL functions through `DrvGetProcAddress`

`wglGetExtensionsStringARB`, `wglCreateContextAttribsARB`,
`wglGetPixelFormatAttribivARB`, `wglGetPixelFormatAttribfvARB`,
`wglChoosePixelFormatARB`. Errors are set with `SetLastError`:
`ERROR_INVALID_VERSION_ARB` (0x2095), `ERROR_INVALID_PROFILE_ARB` (0x2096),
`ERROR_INVALID_PARAMETER`, `ERROR_INVALID_PIXEL_FORMAT`.
