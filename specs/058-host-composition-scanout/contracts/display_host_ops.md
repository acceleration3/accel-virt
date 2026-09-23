# Contract: the four display operations the device asks of QEMU (058)

Added to `RvkHostOps` (`src/device/abi/rvk_device.h`) and mirrored on `Host`
(`src/device/host/host.hpp`). Like every other member of that table they are
required, and like every other one the glue's implementation is a translation
with no decision in it: the format has already been turned into a DRM fourcc by
`ScanoutFormat`, and the geometry has already been checked against the blob by
`ScanoutFramebuffer`.

## The frame description

```c
/* One plane of one scanout, as qemu_dmabuf_new takes it. Every field has been
 * checked against the blob the descriptor covers. */
typedef struct RvkScanoutFrame {
    uint32_t width;           /* the picture, in pixels */
    uint32_t height;
    uint32_t x;               /* its origin inside the backing */
    uint32_t y;
    uint32_t backing_width;   /* the framebuffer the picture is cut from */
    uint32_t backing_height;
    uint32_t stride;          /* bytes per row, <= INT32_MAX */
    uint32_t offset;          /* bytes from the start of the descriptor */
    uint32_t fourcc;          /* a DRM fourcc */
    uint32_t reserved;        /* 0 */
    uint64_t modifier;        /* DRM_FORMAT_MOD_LINEAR today */
} RvkScanoutFrame;
```

## The operations

```c
    /* Main loop. Whether display head `head` exists and its console can show a
     * dma-buf. Asked once per device, when the guest first reads the capset:
     * QEMU registers its display backends after every device is realized
     * (system/vl.c, qemu_init_displays after qmp_x_exit_preconfig), so at
     * realize the answer is no on every host. */
    int (*display_takes_dmabuf)(void *opaque, uint32_t head);
    /* Main loop. Head `head` shows `frame` out of `fd`. The glue duplicates
     * the descriptor; the caller goes on owning its own. Nonzero when the host
     * would not take it, having changed nothing. */
    int (*show_scanout)(void *opaque, uint32_t head, int fd, const RvkScanoutFrame *frame);
    /* Main loop. The rectangle of head `head` that changed. A head showing
     * nothing ignores it. */
    void (*update_scanout)(void *opaque, uint32_t head, uint32_t x, uint32_t y, uint32_t width,
                           uint32_t height);
    /* Main loop. Head `head` shows nothing; the duplicate taken by
     * show_scanout is closed. Safe on a head showing nothing. */
    void (*hide_scanout)(void *opaque, uint32_t head);
```

## What the glue does with each

| Operation | QEMU calls |
|---|---|
| `display_takes_dmabuf` | the head's `QemuConsole` exists, and `qemu_console_has_gl(con)` |
| `show_scanout` | `qemu_dmabuf_new(...)` over `dup(fd)`; `qemu_console_resize(con, width, height)`; `qemu_console_gl_scanout_dmabuf(con, buf)`; the device is marked enabled; the previous `QemuDmaBuf` is released with `qemu_console_gl_release_dmabuf` and freed |
| `update_scanout` | `qemu_console_gl_update(con, x, y, width, height)` |
| `hide_scanout` | `qemu_console_gl_scanout_disable(con)`, then release and free the `QemuDmaBuf` |

Three of those lines are worth their own sentence, because each is a rule of
QEMU's display ABI rather than a choice:

- **`qemu_console_resize` before the scanout.** A GL display's listener blits
  into the console's `DisplaySurface`, and `egl-headless` does nothing at all
  when there is none (`ui/egl-headless.c`, `egl_scanout_flush`:
  `if (!edpy->guest_fb.texture || !edpy->ds) return;`). Stock virtio-gpu calls
  it in the same place (`virtio_gpu_update_dmabuf`).
- **The old buffer is freed after the new one is set**, never before: the
  listener holds an import of it until it is told otherwise, which is the order
  `virtio_gpu_update_dmabuf` uses too.
- **The descriptor is duplicated.** `qemu_dmabuf_new` keeps the fd and
  `qemu_dmabuf_close` closes it, so handing over the core's own would make two
  owners of one descriptor. The core's `GuestDmaBuf` is released when the head
  is unbound, whatever QEMU has done with its copy.

## What the mock must be able to do

`MockHost` and `MockHostOps` answer these, and a test can make each fail:

- `display_takes_dmabuf` answers no for every head, for a chosen head, or yes
  for a chosen count of heads - the three shapes FR-004's fallback has.
- `show_scanout` refuses, so the refusal path has a caller.
- Every call is recorded with its arguments, so a test asserts *what was shown*
  rather than that something was: the head, the descriptor, and every field of
  the frame.
- The contract is checked, not assumed: an `update_scanout` for a head showing
  nothing is a recorded violation, as is a `show_scanout` naming a descriptor
  that is already closed, a `hide_scanout` of a head showing nothing after one
  that already hid it, and any of the four called off the main loop.

A `show_scanout` for a head that already shows something is **not** a
violation: it is how a head changes what it shows without a blank frame
between, which US2 scenario 2 requires.
