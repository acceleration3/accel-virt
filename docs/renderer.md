# The renderer process

The host GPU stack - DXVK, vkd3d-proton, the Vulkan, OpenGL and OpenCL
drivers - runs guest-supplied shaders and command streams. A crash inside it
used to take QEMU, and the VM, with it. With `renderer=` set, the device runs
that stack in a separate process, `av-renderer`: a crash there loses the
device's contexts, which the guest sees as a device loss it already recovers
from, and QEMU starts a new renderer.

## Where the line is

The whole per-context worker moves: ring consumer, batch executor, executor
library, blob imports, fence reporter, D3D ordering, and the device's domain in
the library, where the objects the contexts share are kept: the renderer opens
it as QEMU's hello reaches it. QEMU keeps
the control plane - the virtqueue, the authoritative `BlobTable`, the hostmem
BAR, the display heads, doorbell ioeventfds and guest notifiers.

Nothing crosses the process boundary per batch. The guest's ring, fence page
and reply slot are guest RAM, which both processes map; the doorbell is a KVM
ioeventfd and the fence interrupt a KVM irqfd, kernel objects whichever process
holds them. Cutting at the executor ABI instead would cost a round trip per
command (about 48 for one D3D12 ExecuteCommandLists) plus every `poll` - more
than the frame.

One renderer serves one device: D3D ordering, OpenGL share contexts, shared
objects and the D3D11 immediate context all span contexts, and EGL sharing
works only inside one process. A crash therefore loses every context on the
device, as an adapter-wide reset does on Windows.

## What the renderer is given

Only file descriptors and bytes, over one `SOCK_SEQPACKET` socketpair:

| Need | How the renderer reaches it |
|---|---|
| Guest RAM | Each RAMBlock's memfd (`share=on` is required), mapped once |
| Guest blobs | Mirrored into its own `BlobTable`, extents as `(region << 40) \| offset` |
| Host blobs | The hostmem memfd QEMU made and placed |
| Doorbell | The eventfd QEMU created and registered as the ioeventfd |
| Fence interrupts | Each queue's guest-notifier eventfd |
| Scanout images | Sent back as a dma-buf when the image changed (phase 2) |

QEMU never registers a descriptor the renderer made with KVM, and accepts only
dma-bufs back.

A device with a renderer exports no surface on its export socket
(docs/external-export.md): the surfaces are the renderer's, and nothing asks
it for their descriptors yet.

## Lifecycle

QEMU spawns the renderer at realize and waits for `Ready`, which carries the
sub-APIs and import kinds the capset needs. It watches the socket and a pidfd.
When the renderer dies or misses a request's deadline, every bound context's
fence page is republished with `device_lost` and its interrupt raised, the
doorbells are disconnected, and a new renderer is started, which is replayed
the regions, live blobs and interrupt lines before it takes a context. A
renderer that dies too often in a short time is not restarted; new contexts
then fault with `no_executor`.

## Phases

1. The renderer runs every context; Vulkan, OpenGL and OpenCL end to end;
   crash detection, fault and respawn. Scanout export answers false.
2. Scanout images and `read_image` through the renderer; D3D12 measured
   against the in-process device.
3. Asynchronous destroy and bind answers, a distinct fault code, and the GPU
   catalog and probes moved out of QEMU.
