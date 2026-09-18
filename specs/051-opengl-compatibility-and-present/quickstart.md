# Quickstart: OpenGL compatibility profile and presentation (051)

## Unit and loopback (no GPU)

```bash
cmake --build build && ctest --test-dir build -LE tools -R 'gl|opengl'
```

Proves: the ask ladder and what each rung reports, the legacy gate on a core
context, the interval's refusals, a swap on an unseen window costing nothing,
a front-buffer read, and the pacer in its new home.

## With this host's real GPUs

```bash
ctest --test-dir build -R 'opengl_backend_device|fullstack_opengl'
```

Proves: both host GPUs grant a 4.6 compatibility context and report mask 2; a
display list compiled on the host is called with one command; a compatibility
frame lands in `build/images/opengl-compat.bmp`.

## In the guest

```bash
AV_GUEST_IMAGE=~/.local/share/libvirt/images/win11.qcow2 \
  python3 tools/guest/run_guest_suite.py --real --quick
```

Proves live: `glGetString(GL_VERSION)` names a compatibility profile,
`wglSwapIntervalEXT(1)` paces within the tolerance 046 set, a resized window
keeps presenting, and the suite's GL frames match their references where a
reference is pinned.
