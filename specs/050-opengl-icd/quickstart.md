# Quickstart: proving the OpenGL ICD

## The first live experiment (before the ICD is built)

```bash
tools/guest/run_guest_suite.py --image "$AV_GUEST_IMAGE" --quick --opengl-discovery
```

A logging-only `rvkgl.dll` (the 14 exports, each writing its name) is installed
with the INF's three values; `rvkglprobe.exe --discovery` makes a session-0
window, calls `SetPixelFormat` and `wglCreateContext`, and prints what it got.
Expected: `C:\ProgramData\rvk\rvkgl-<pid>.log` lists `DrvValidateVersion 2`,
`DrvSetCallbackProcs 9`, `DrvDescribePixelFormat`, `DrvSetPixelFormat`,
`DrvCreateContext`. Anything else is recorded in tasks.md and the
interactive-session launcher (research §9) is built before Phase 3.

## On Linux, in seconds

```bash
cmake --build build
ctest --test-dir build -E tools_tests -j8       # ICD classes, the fake opengl32, KMD/device/executor units
ctest --test-dir build -R test_fullstack_opengl_icd --output-on-failure
python3 tools/check_generated.py                  # the proc table order and the emitter change
```

`test_fullstack_opengl_icd` drives the ICD's `Drv*` entry points as opengl32
does, over the real driver, device and executor on this machine's GPU, with a
scripted window: every probe step passes, the drawn triangle's pixels arrive in
the scripted blit, the shared texture is read in the second context, and a
forced fault leaves the context lost and a new one working.

## In the guest

```bash
python3 tools/re/gl_proc_table.py --check          # the committed order matches the pinned opengl32.dll
AV_GUEST_IMAGE=... tools/gate.sh                   # includes the --opengl boot
```

Expected in `build-guest-opengl/`: `opengl.txt` with
`rvkglprobe: N passed, 0 failed` and `ok opengl.<step>` for pixel_format,
context, version, draw, share, attribs, busy, lost, recreated; `GL_RENDERER`
naming the host GPU; the ICD's log in `diag/` stating the buffer-mapping path.
