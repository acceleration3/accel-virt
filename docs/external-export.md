# Handing shared surfaces to a process outside QEMU

A process on the host - LSW's window mirror, remotewin's host half - can be
handed a surface a guest application shared, as the host GPU memory the
surface already lives in: a dma-buf, with a sync_file for the copies that
wrote it. Nothing is copied on the host, and in the guest nothing but the
application's own `CopyResource` into the surface. The pixels never cross the
guest's network or memory.

What it replaces, for remotewin: Windows.Graphics.Capture frames read back to a
CPU staging texture in the guest and sent over QEMU's user-mode network, about
116 MB/s, so a 2560x1440 window ran at about 8 frames a second.

## 1. The shape of it

```
guest application (remotewin-guest.exe)          host process (remotewin host)
  N shared textures, ids learnt once     ---- ids, over its own link ---->
  CopyResource(frame -> slot k), Flush   ---- "slot k, copy n" -------->
                                                    |
                                    export socket   v   (once per slot)
  virtio-gpu-av device  <------- EXPORT(id) ------- consumer
    executor: the texture's            -------- dma-buf + layout ------>
    memory, exportable from            <------- SYNC(id) ---------------  (per frame)
    the start (ExportableImages)       -------- copies run + sync_file ->
                                       -------- REVOKED(id) ------------  (guest released it, or reset)
```

The guest and the consumer agree on which surfaces and which frames over a
link of their own (remotewin's). The device knows nothing of that link; it
answers for the surfaces the guest shared.

## 2. The socket

`-device virtio-gpu-av-pci,...,export-socket=/run/user/1000/lsw-export.sock`

- **What listens.** QEMU's glue, on QEMU's main loop: a `SOCK_SEQPACKET` unix
  socket at the path, mode 0600 once bound. Each connection accepted is handed
  to the device core, which serves it on a thread of its own; the main loop
  only accepts.
- **Who is served.** A process whose user is QEMU's own (`SO_PEERCRED`); any
  other is closed at once. At most 8 connections at a time; a ninth is closed
  at once. A connection whose consumer has gone is reaped when the next one
  arrives.
- **The path.** A socket file a QEMU that is gone left behind (connect answers
  `ECONNREFUSED`) is replaced. A path another live process listens on, a path
  that is not a socket, and a path too long for `sun_path` fail realize, and
  nothing at the path is touched. QEMU removes the file when the device is
  finalized; a QEMU that is killed leaves it, and the next one replaces it.
- **The sandbox.** The glue uses `socket`, `bind`, `chmod`, `listen`,
  `accept4`, `getsockopt`, `connect`, `unlink`, `recvmsg`, `sendmsg`,
  `shutdown` and `close`, and the core one QEMU thread per connection. None is
  in QEMU's seccomp deny lists (`system/qemu-seccomp.c`: `spawn=deny` refuses
  `fork`, `vfork`, `execve` and a `clone` without `CLONE_THREAD`;
  `elevateprivileges`, `resourcecontrol` and `obsolete` refuse no socket call).
  `tests/qemu/test_virtio_gpu_export.py` runs every case under
  `-sandbox on,obsolete=deny,elevateprivileges=deny,spawn=deny,resourcecontrol=deny`.
- **Where it answers.** Only a device whose contexts run in QEMU: with
  `renderer=` set the surfaces live in av-renderer's process, and every
  request is answered `unavailable` (status 5). So is every request to a
  device whose executor library is older than ABI 15.
- **Backpressure.** The device never waits to send. A consumer whose socket is
  full when an answer or an event is due is taken to have gone: its connection
  is shut down. Read every datagram.

## 3. Messages

Little-endian, fixed size, one datagram each; each starts with a magic and a
version. This document is version **1.2**. A device reads any minor of major 1
as this layout and answers a request of another major `not_supported`. A
consumer should do the same with answers. Version 1.1 added timing (§4),
and 1.2 head (§4); an older device answers each as a request it cannot read.

### 3.1 Request (consumer -> device), 32 bytes

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVXQ` = `0x51585641` |
| 4 | u16 | major | 1 |
| 6 | u16 | minor | 2 |
| 8 | u32 | operation | 1 hello, 2 export, 3 sync, 4 release, 5 timing, 6 head |
| 12 | u32 | reserved / refresh | 0; for timing, the consumer's display's refresh in millihertz, 1000 to 1000000 |
| 16 | u64 | surface / blank | the guest's shared id (§5); 0 for hello; for head, the head plus one, 1 to 16. For timing, a moment on the host's `CLOCK_MONOTONIC`, in ns and not 0, that the guest's display is to blank in phase with |
| 24 | u64 | tag | the consumer's own; the answer carries it back |

A consumer sends no descriptors; any that come are closed. A request may be
sent before the last one's answer arrives: answers come in request order, on
the one connection.

### 3.2 Answer and event (device -> consumer), 64 bytes

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `AVXA` = `0x41585641` |
| 4 | u16 | major | 1 |
| 6 | u16 | minor | 1 |
| 8 | u32 | operation | the request's; 0 for one that could not be read; `0x80` revoked (an event) |
| 12 | u32 | status | 0 ok, 1 malformed, 2 not_supported, 3 not_found, 4 no_space, 5 unavailable |
| 16 | u64 | surface | the request's (0 for hello, for timing and for one that could not be read); an event's surface |
| 24 | u64 | tag | the request's; 0 in an event and for a request that could not be read |
| 32 | 32 bytes | body | by operation, below; all zero in any refusal |

Bodies of an answer with status ok:

| Operation | Body | Beside it |
|---|---|---|
| hello | 16 bytes GPU UUID (`VkPhysicalDeviceIDProperties::deviceUUID`), u32 PCI domain, u8 bus, u8 device, u8 function, u8 flags (bit 0: on PCI; otherwise the address is 0), 8 bytes reserved | nothing |
| export, head | u32 width, u32 height, u32 DRM fourcc, u32 stride (bytes), u32 offset (bytes), u32 reserved, u64 DRM format modifier | the dma-buf |
| sync | u64 copies run, 24 bytes zero | the sync_file |
| release | zero | nothing |
| timing | zero | nothing |
| revoked (event) | u32 reason (1 the guest released the surface, 2 the device was reset), 28 bytes zero | nothing |

A descriptor travels with `SCM_RIGHTS`, exactly one beside an ok export, sync
or head and none otherwise. It is the consumer's to close.

`src/proto/export/` holds both encoders and decoders (`ExportRequest`,
`ExportAnswer`), with every rule above, a hostile sweep each and
`tests/fuzz/fuzz_export_messages.cpp`.

## 4. What each operation does

- **head** composes what the head shows now - its overlay planes (protocol.md,
  "Scanout planes") drawn by layer into one image of the head's size - and
  answers it as a linear B8G8R8A8 dma-buf (modifier `DRM_FORMAT_MOD_LINEAR`)
  the consumer may map. Each request composes again into the same image, so a
  consumer reads one before it asks for the next; nothing is held, and nothing
  is revoked. `not_found` for a head with no plane, `unavailable` where the
  device composes none (`renderer=`, or an executor without `compose_planes`).

- **hello** names the GPU the surfaces are on, so the consumer imports on the
  same one: match the UUID against `VkPhysicalDeviceIDProperties::deviceUUID`,
  or the PCI address against the render node's
  (`/sys/class/drm/renderD*/device`). Status 5 says this device exports
  nothing.
- **export** hands over the surface's own memory: the dma-buf of the image the
  host made the guest's texture as. The connection holds the surface from then
  on, as many as it exports; `no_space` only when the device has no memory to
  record one more, and a surface already held is answered again with a fresh
  descriptor of the same memory. `not_found` for an id no
  guest process shares, or one whose memory no other API can open (§5.1).
- **sync** answers how many copies into the surface the host has run since the
  guest shared it, and a sync_file that signals once the GPU has finished
  every one of them - with everything else the host's Direct3D 11 device was
  given before the question. `not_found` for a surface this connection does
  not hold.
- **release** lets the connection's hold go; ok whether it held it or not.
  Close the dma-buf too.
- **revoked** comes unasked to every connection holding the surface, after
  the guest let go of the surface everywhere (the miniport's
  `SharedReleaseRequest`, docs/protocol.md 0x02F1), and to every connection
  with surface 0 when the device is reset (the guest's ids start again). The
  connection holds it no more. The dma-buf the consumer has still names
  memory - a dma-buf keeps its pages - but nothing will write it again and the
  id may be minted afresh for another surface: drop the import, close the
  descriptor, and stop using the id.

- **timing** puts the guest's display clock in step with the consumer's
  display (docs/protocol.md, "Display clock"): from each head's next blank
  on, the head blanks at the refresh given, each blank a whole number of
  periods from the moment given. The next blank is the first such one at
  least half a period after the last and still to come, so the guest never
  sees two blanks closer than that, and the sequence carries on. `unavailable`
  from a device that is not realized. The consumer chooses both: remotewin
  gives the whole number of its output's periods nearest the guest mode's 60 Hz,
  and puts the guest's blanks just far enough ahead of its own that each
  frame the guest composes reaches the consumer's compositor in time for the
  next one (remotewin's `DisplayLock`). With more than one consumer, the last
  to ask wins.

### 4.1 Which copy a sync covers

The guest cannot see when the host runs its commands, so a frame is named by
a count. The host counts every `CopyResource` and `CopySubresourceRegion`
whose destination is the surface, from any Direct3D 11 session, in the order
it runs them, under the device lock the copies run under; a sync reads the
count and makes its sync_file under the same lock. So:

- the guest counts its own copies into slot k: the n-th is "slot k, copy n";
- the consumer asks `sync(k)`: an answer of `copies >= n` means the n-th copy
  has run, and the sync_file signals once the GPU has finished it - wait on
  it on the consumer's GPU queue (EGL `EGL_ANDROID_native_fence_sync` and
  `eglWaitSyncKHR`; Vulkan: import it into a binary semaphore with
  `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT`, temporary) or on the CPU
  (`poll` for `POLLIN`);
- `copies < n`: the guest's copy has not reached the host yet - its batch is
  still in the guest's ring. Ask again a little later (a millisecond); the
  device never holds an answer back.

Only copies are counted: draws, clears and `UpdateSubresource` into the
surface are not. Direct3D 9 and 12 writes are not counted either. A guest
that would rather not count can wait instead: an `ID3D11Query` of
`D3D11_QUERY_EVENT` ended after the copy reports done once the host GPU has
finished it, and any sync after that covers it.

Each sync costs the host a flush of the Direct3D 11 immediate context and an
empty submission on DXVK's queue (`DxvkVulkanDevice::sync_file`).

## 5. The guest side

### 5.1 Which textures can be exported

A Direct3D 11 texture made with `D3D11_RESOURCE_MISC_SHARED` (the legacy
kind; with `SHARED_NTHANDLE` beside it the id should be read the same way
through `IDXGIResource1::CreateSharedHandle` and
`D3DKMTOpenResourceFromNtHandle`, as the guest's own ICDs do - not tried
here), 2D, one mip, one array layer, one sample,
usage default, binds no more than `SHADER_RESOURCE | RENDER_TARGET`, in
`B8G8R8A8_UNORM(_SRGB)`, `R8G8B8A8_UNORM(_SRGB)`, `R10G10B10A2_UNORM` or
`R16G16B16A16_FLOAT`. avumd.dll makes such a texture on the host over memory
other APIs can open (`ExportableImageInfo`, `ExportableImages::make`), which is
what is exported. Any other texture is `not_found`.

### 5.2 The id, from user mode

The id is the one the miniport minted over the texture's allocation
(docs/shared-surfaces.md §1.1). It is in the allocation's private driver data,
which dxgkrnl hands every process that opens the resource, so a process reads
its own texture's id by opening it through D3DKMT - no escape, no driver call
of this project's: avgpu.sys needs no change.

The D3DKMT entry points must be gdi32's (win32u's underneath). d3d11.dll
exports stubs of the same names that succeed and do nothing:
`D3DKMTCreateDevice` through them answers success with device 0, and every
call after it fails. A linker given `-ld3d11` before `-lgdi32` binds them
there, so link gdi32 first, or look the entry points up in gdi32.dll at run
time (measured in a Windows 11 guest, 2026-10-05).

1. `IDXGIResource::GetSharedHandle` on the texture: a legacy shared handle,
   which is a `D3DKMT_HANDLE` (global share).
2. `IDXGIDevice::GetAdapter`, `IDXGIAdapter::GetDesc`: the adapter's LUID;
   `D3DKMTOpenAdapterFromLuid` and `D3DKMTCreateDevice` on it (once per
   process).
3. `D3DKMTQueryResourceInfo { hDevice, hGlobalShare }`: the sizes. One
   allocation; its private data is 96 bytes.
4. `D3DKMTOpenResource` with buffers of those sizes and one
   `D3DDDI_OPENALLOCATIONINFO`: its `pPrivateDriverData` points at the
   allocation's 96 bytes.
5. At byte 56 of them is avgpu's record: u32 magic `0x00535641` ("AVS"), u32
   version 1, **u64 the id at byte 64**, u32 kind, u32 reserved, u64 adapter,
   u64 size. Anything else is not avgpu's surface.
6. `D3DKMTDestroyAllocation { hDevice, hResource }` lets go of the open.

`tools/guest/probes/shared_id_probe.cpp` is that recipe whole, with the
`d3dkmthk.h` structures MinGW does not ship, built by
`tools/guest/probes/build.sh`. All the calls are gdi32's exports.

The id stays the texture's until every process lets go of the texture; then
the miniport tells the device, and the consumer is told (§4).

### 5.3 A mirror's frames

1. Once, per window: N shared textures (N = 3 is plenty) of the window's
   size, their ids (§5.2) sent to the host half. Resize: new textures, new
   ids; release the old ones.
2. Per frame: the Windows.Graphics.Capture frame copied into a slot k the host
   is not reading (`CopyResource(slot[k], frame)` - one copy, counted), then
   `Flush`. Tell the host half "slot k, copy n_k".
3. The host half, once per slot: `export(id_k)`, import the dma-buf (§6).
   Per frame: `sync(id_k)` until `copies >= n_k`, wait on the sync_file,
   draw from the import, and tell the guest when it has finished reading slot
   k (its GPU work on it done) so the guest may copy into it again. Nothing in
   the device keeps a guest copy off a slot the consumer is still reading.

## 6. The host side: importing

The dma-buf is the texture's own memory, bound to memory of its own
(dedicated) and laid out linear: an implicit layout is one only some drivers
keep with the memory - RADV writes it into the buffer's metadata, Intel's i915
and Xe do not, and an importer there read the picture scrambled - so the image
is made with `VK_IMAGE_TILING_LINEAR` where the GPU can export it so
(`ExportableImages`), and the modifier is `DRM_FORMAT_MOD_LINEAR` (0). It is
still imported on the same GPU (§4, hello).

- **EGL** (Qt's OpenGL, Mesa radeonsi): `eglCreateImageKHR(display,
  EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, { EGL_WIDTH, w, EGL_HEIGHT, h,
  EGL_LINUX_DRM_FOURCC_EXT, fourcc, EGL_DMA_BUF_PLANE0_FD_EXT, fd,
  EGL_DMA_BUF_PLANE0_OFFSET_EXT, offset, EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
  EGL_NONE })` - with no modifier attributes, which a linear buffer needs none of - then
  `glEGLImageTargetTexture2DOES`. The display must be the GPU hello named
  (`EGL_EXT_platform_device` / `EGL_EXT_device_drm`).
- **Vulkan** (same GPU): make the image exactly as `ExportableImageInfo`
  describes it - 2D, the format, one mip, layer and sample,
  `VK_IMAGE_TILING_OPTIMAL`, usage `TRANSFER_SRC | TRANSFER_DST | SAMPLED |
  COLOR_ATTACHMENT`, `MUTABLE_FORMAT` with the format and its sRGB sibling in
  a `VkImageFormatListCreateInfo` for the 8-bit formats, external memory
  `DMA_BUF` - and import the fd as its dedicated memory. RADV lays out an
  image the same from the same description; that is how the guest's own
  Vulkan opens these textures (`test_d3d_vulkan_interop_device`).

DRM fourccs: `AR24` for B8G8R8A8, `AB24` for R8G8B8A8, `AB30` for
R10G10B10A2, `AB4H` for R16G16B16A16 float; an sRGB texture has its linear
sibling's.

## 7. Why the surface's own memory, and what it costs

Measured on an RX 9070 XT (RADV, Mesa 26.2.4) by
`tests/unit/executor/d3d/test_d3d_external_export_device.cpp`, at 2560x1440,
240 frames, each the guest's copy into the surface and then:

| Path | Per frame |
|---|---|
| This: sync, and the sync_file waited for | 0.10 - 0.13 ms |
| A copy into a linear exportable image (`HostImageExport`, the scanout's path), waited for | 0.22 - 0.24 ms |

The direct export saves a copy of the whole frame on the GPU (about 0.1 ms and
15 MB of bandwidth a frame here) and leaves the consumer reading the tiled,
compressed layout the GPU drew in rather than a linear one. The same test
imports the export with EGL at 256x128, at 1003x701 (a pitch of 4096 bytes -
the driver's, not the width's) and at 2560x1440, and finds every texel where
Direct3D put it.

What the linear layout gives up is the GPU's own tiling, which composition
reads faster; what it gains is a picture every driver reads the same.

## 8. Where it lives

| Piece | Where |
|---|---|
| The messages | `src/proto/export/`: `ExportRequest`, `ExportAnswer`, `ExportedLayout`, `ExportGreeting` |
| Connections, their threads, revocation | `src/device/export/`: `ExportService`, `ExportConnection`, `ExportedGpu` |
| The socket and its connections' I/O | the glue, `src/device/qemu/virtio-gpu-av-host.c`, through `AvHostOps` (`receive_export`, `send_export`, `shutdown_export`, `close_export`) and `av_device_export_accepted` |
| The device property | `export-socket`, `src/device/qemu/virtio-gpu-av.c` |
| Timing | `ExportConnection` to `DisplayClock::align`, `VblankSchedule::align` |
| Revocation | `SharedCommands` (0x02F1) and `VirtioGpuAv::reset` |
| The executor | ABI 16, `export_surface` and `sync_surface` in the device's domain (`av_executor.h`, docs/protocol.md); `ExecutorRoot`, `D3d11HostDevice`, `ExportableImages::export_image`, `DxvkVulkanDevice::sync_file`; the surface and its counted copies in `HostShares`, counted by `D3dBackend` |
| Tests | the units under `tests/unit/proto/export/` and `tests/unit/device/export/` (a real socketpair, `FakeExportConsumer`); `test_d3d_external_export_device` on the GPU; `tests/qemu/test_virtio_gpu_export.py` against QEMU under seccomp, with the echo executor and this project's |
