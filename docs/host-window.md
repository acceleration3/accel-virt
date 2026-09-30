# The host window

Each device has a hostmem BAR: `hostmem=8192M` gives an 8 GiB window of guest
physical addresses that QEMU backs, page range by page range, with host memory
it places there. This is how the driver hands processes memory that the guest
CPU reaches write-back and the host reaches without importing guest pages.

## How it works

The kernel driver owns the window. VidMm never places anything in it.

1. **Placement.** `HostWindow` is a page bitmap over the window. An allocation
   takes the first fit searched from a fixed base, never from a pointer that
   only moves forward. Every range ever mapped into a process costs that
   process's page tables for the rest of the boot, so a workload that creates
   and destroys chunks should keep reusing one small part of the window.
2. **Backing.** For each chunk, the driver makes a host blob
   (RESOURCE_CREATE_BLOB, HOST3D, mappable). Its resource id is one of the
   rvk context's chunk slots. The driver then places the blob at the chosen
   offset with MAP_BLOB and waits for the answer. When the escape returns,
   the window range is backed.
3. **Mapping.** The driver maps the BAR pages of that range into the calling
   process write-back, and gives the process the address. The host maps the
   same memfd pages write-back. A write-combined guest mapping of pages the
   host maps write-back is an aliasing the SDM leaves undefined, and it reads
   stale data.
   A 32-bit process can address only its low 4 GiB, so a view that would end
   above that is taken back and the chunk goes to guest RAM instead (below).

4. **Release.** The driver unmaps the process's view, UNMAP_BLOB and UNREF go
   to the device and are answered, and only then does the range return to the
   bitmap. If the range were handed out again earlier, the next blob could be
   placed over a mapping QEMU still holds.

All of this runs in one escape, `HostWindowRecord`, which a process sends on
its rvk context. There is an `open` (bytes → resource, address) and a `close`
(resource). A context that is destroyed closes every chunk it still holds. If
the owning process has already exited, its view is gone, and only the host
side is released.

## When the window cannot hold a chunk

The same `open` answers with a chunk of guest RAM when the adapter has no
window, the window has no room for the chunk, the device will not place a host
blob there, or the process cannot be given a view within its reach. The
driver allocates the pages itself, maps them into the process write-back, and
publishes them as a `GUEST` blob in the same chunk slot, which the device
imports as it imports any guest pages. The process sees the same answer: a
resource, whole pages of at least what it asked, and an address. Only a device
that will back the chunk nowhere refuses it, with `STATUS_NOT_SUPPORTED`.

The pages are the driver's, not an allocation of the process's, for the reason
a context's ring is (docs/protocol.md, "Context memory"): VidMm evicts a
suspended process's allocations and maps them back at other pages, under a
device that had imported the old ones. A `close` sends `RESOURCE_UNREF`; one
the device refuses - an import still holds the blob - leaves the chunk bound
until its context goes, so a process that closes early cannot make the driver
keep page after page out of use. Pages the device may still hold when the
context goes are kept out of use for good rather than freed.

Why not VidMm: on WDDM 3.x, VidMm refuses a CPU-visible allocation whose
segment set has no aperture segment. With the aperture in the set, VidMm hands
out the aperture's staging pages at Lock rather than the window. So a driver
that wants the window has to place blobs in it itself.

## On the host

A host blob is a memfd, sealed against growing and shrinking. The seal is what
lets udmabuf wrap it. The device imports a window chunk the same way as a
guest one:

- a dma-buf over the memfd range, which host GPUs take even though the pages
  are file-backed;
- or else the memfd's own mapping, as a host pointer.

In renderer mode, the renderer maps the same memfd and makes the dma-buf
itself.

## Per API

| API | What uses the window | Effect |
|---|---|---|
| D3D11 | The session arena: Map/Unmap uploads and readbacks | Readbacks read write-back memory instead of write-combined |
| D3D12 | The session arena, and upload/readback heaps carved from it | Readback heaps read write-back memory; the GPU reads the same host RAM through the dma-buf |
| OpenGL | The context arena: buffer uploads and readbacks | As D3D11 |
| OpenCL | The context arena, and `ALLOC_HOST_PTR` buffers made over it | Mapped buffers are write-back |
| Vulkan | `HOST_VISIBLE` chunks | The mapping matches the `HOST_CACHED` types the host advertises |

A chunk that the window cannot hold, or a device with no window, is guest RAM
the driver owns, reached the same way.

## Budgets

The adapter reports the host GPU's local memory to VidMm as a second segment
(`LocalMemorySegment`): a memory segment with no flags, the size the capset's
`local_memory` gives. VidMm counts it as the adapter's dedicated video memory,
so dxdiag, DXGI's `DedicatedVideoMemory` and D3DKMTQueryVideoMemoryInfo's
local budget describe the host GPU rather than guest RAM. Nothing is ever
placed in it: every allocation names the aperture or VidMm's own system memory
segment. Vulkan's device-local heaps are still sized from the host's own heaps:
what lives there is host GPU memory, which the guest never maps. Every other
heap is the adapter's non-local budget.
