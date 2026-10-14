# The compositing context

Each host API the executor drives runs on a Vulkan device of its own:

- DXVK's Direct3D 11, one per GPU;
- a DXVK Direct3D 9 device per guest device;
- vkd3d-proton's Direct3D 12.

Until this branch, a picture one of them drew reached another through the
CPU: it was read back into host memory, then uploaded again. The compositing
context lets the picture cross on the GPU instead.

## What it is

One `CompositorDevice` per host GPU (`src/executor/shim/compositor_device.hpp`).
It is a `VkInstance` and `VkDevice` of the executor's own, on the GPU the
guest's adapter stands for, with external memory (fd and dma-buf), external
semaphore fd and timeline semaphores. `LoaderHostApis` opens one for each
GPU as it opens the GPUs, builds the GPU's `HostMemorySource` over it, and
hands it to `D3d11HostDevice::use_compositor`.

The compositor does not draw. It owns the memory the guest maps, the images
the APIs hand each other, and the timelines they order their work by:

- Every byte the guest CPU and the GPU share is allocated here
  (docs/host-memory.md). `CompositorMemory` allocates exportable
  `VkDeviceMemory` for the memory source, exports it once as a dma-buf and
  maps it through `vkMapMemory`; QEMU maps the dma-buf into the hostmem
  window, and each API imports it. Nothing is allocated until a guest asks,
  and each host-visible type is proved - a small allocation made, exported
  and mapped through the export - before it is first offered.
- An image made here is laid out linear over memory exported as a dma-buf. It
  is built from the same `ExportableImageInfo` description on every device,
  so each device imports it as an image of its own over the same memory, and
  OpenGL imports it as a memory object.
- A timeline made here is exported as an opaque fd. It is imported by the
  device that writes the image and by the one that reads it.

## The reusable pieces

| Class | Does |
|---|---|
| `VulkanDeviceTable` | One device's entry points, loaded once. Images, memory, semaphores, command buffers, barriers and copies, over any `VkDevice`: the compositor's, DXVK's or vkd3d-proton's. |
| `VulkanImage` | An image over exportable memory: made here (`make_exportable`), or imported from a dma-buf (`import_dma_buf`, which owns the fd whatever happens). |
| `VulkanTimeline` | A timeline semaphore: made and exported, or imported from an fd. |
| `BorrowedQueue` | A queue another library owns: its handle, family, and the lock that library takes to submit. `DxvkD3d11Queue` and `DxvkD3d9Queue` borrow DXVK's through its interop interfaces. A vkd3d-proton queue would be the third. |
| `QueueWork` | Copies and waits recorded on a borrowed queue, under its lock, from a ring of command buffers that are reused once their timeline value is reached. |
| `D3d9VulkanLane` | A Direct3D 9 device's Vulkan side: its table, its queue, and a surface's image and layout, with DXVK's deferred clears forced first. |
| `DxvkVulkanDevice` | The same for Direct3D 11: imports a timeline and waits on it on DXVK's queue. |

OpenGL and OpenCL do not take these classes. They import the same dma-buf
and timeline fds through their own extensions.

## Threads

There is one submission point per queue, the library's own, and work is
submitted under that library's lock. Nothing spins. Work across devices is
ordered by GPU semaphores and timelines, not by a CPU wait. No thread is
added to copy bytes.

## What crosses on the GPU now

- **A fullscreen Direct3D 9 flip** (`D3d9FlipShares::flip_on_gpu`).
  - The back buffer is copied on Direct3D 9's queue into an image the
    compositor made, and Direct3D 11 imports the same memory as the shared
    primary.
  - Direct3D 11's queue waits on the timeline before it reads the primary.
  - Without a compositor the flip crosses through the CPU, as before
    (`flip_on_cpu`).
- **An OpenCL image over a Direct3D 11 texture** (docs/d3d11-sharing.md,
  stage 2). The texture's twin is shared memory that OpenCL opens as an image
  in place. Acquire and release are copies on Direct3D's GPU.

## Windowed presents, as measured

These measurements were made in a Windows 11 guest, with a windowed
blt-model test in Direct3D 9 and in Direct3D 11.

- **DWM opens only keyed surfaces.** It opens what other processes made
  through avumd: DirectComposition surfaces and flip-model buffers, which
  never leave the GPU. It never takes avumd's keyless open path, and it
  writes no pixels into any of them with the CPU.
- **A blt goes into a staging allocation.** The kernel copies a windowed
  blt-model present into a *staging* standard allocation the size of the
  window (`StandardAllocationType` 3, no key). No later blt through avgpu
  reads it: Windows reads it with the CPU. That is VirtualBox's rule too: a
  readback only where the destination is staging that GDI reads. So the frame
  must reach that allocation's pages.
- **What the copy costs now.** The frame is staged on the host GPU at a present
  point, and the present engine makes the one CPU copy into the pages.
  - Direct3D 11 and 12 name the point in the present's private data.
  - Direct3D 9 names it by escape (`NextPointRecord`), because d3d9.dll drops
    the present's private data. Until this branch, every Direct3D 9 windowed
    frame was read back over the wire, then copied page to page.

## What still crosses the CPU

| Path | Why |
|---|---|
| The staging allocation of a windowed blt | Windows reads it with the CPU (above). One copy, from the host-staged frame into its pages. |
| OpenGL and Vulkan window fallbacks | Used only where a window takes no DXGI chain; the window is drawn with GDI. |
| GDI drawing and the boot console | The pixels are written by the CPU in the guest. |
| An application's own readback | The application asked for the bytes. |
