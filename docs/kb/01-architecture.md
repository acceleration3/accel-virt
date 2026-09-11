# 01 — WDDM 3.2 architecture

## The stack, top to bottom

```
                         USER MODE (per-process)
  ┌──────────────────────────────────────────────────────────────────┐
  │ application                                                      │
  │   └─ d3d12.dll  (thin loader)  →  D3D12Core.dll  (the runtime)   │
  │        └─ dxgi.dll        adapter enumeration, swapchains        │
  │             └─ USER-MODE DRIVER (UMD)   d3d12umddi.h             │
  │                  vendor DLL, e.g. amdxc64.dll / nvwgf2umx.dll    │
  │                       └─ D3DKMT thunks in gdi32.dll              │
  └──────────────────────────────────────────────────────────────────┘
                                   │ syscall
  ┌──────────────────────────────────────────────────────────────────┐
  │ dxgkrnl.sys      "DirectX Graphics Kernel"                       │
  │   ├─ handle/object table: DXGPROCESS, DXGADAPTER, DXGDEVICE,     │
  │   │  DXGCONTEXT, DXGALLOCATION, DXGSYNCOBJECT …                  │
  │   ├─ D3DKMT* entry points (the kernel side of the thunks)        │
  │   ├─ VidPn manager (display topology)                            │
  │   └─ virtualization: DXGVIRTUALGPUMANAGER_{PARAV,GPUP}           │
  │ dxgmms2.sys      VidMm (memory) + VidSch (scheduler)             │
  │   ├─ segments, residency, eviction, paging buffers               │
  │   ├─ GPU virtual address space, page tables                      │
  │   └─ per-node queues, preemption, TDR                            │
  └──────────────────────────────────────────────────────────────────┘
                                   │ DDI  (187 function pointers)
  ┌──────────────────────────────────────────────────────────────────┐
  │ KERNEL-MODE DISPLAY MINIPORT DRIVER (KMD)   ← what we build      │
  │   dispmprt.h + d3dkmddi.h                                        │
  └──────────────────────────────────────────────────────────────────┘
                                   │
                              the hardware
```

The important structural fact: **`dxgkrnl` is the port driver and your KMD is
the miniport.** You never drive the scheduler; the scheduler drives you. Almost
every DDI is a callback *into* you, and the small set of things you can
initiate goes back up through `DXGKRNL_INTERFACE` callbacks (`DxgkCb*`).

## The two miniport shapes

`dispmprt.h` exposes exactly two registration entry points:

| Entry point | Init struct | Shape |
|---|---|---|
| `DxgkInitialize` | `DRIVER_INITIALIZATION_DATA` (187 DDIs) | Full miniport. Render + display, or render-only. |
| `DxgkInitializeDisplayOnlyDriver` | `KMDDOD_INITIALIZATION_DATA` | Display-only (KMDOD). Scanout, no GPU engines. |

`basicdisplay.sys` takes the second path; `basicrender.sys` takes the first.
For GPU acceleration you need the first — a display-only driver has no
contexts, no DMA buffers, no GPU virtual addresses, and D3D12 will not create a
device on it.

## The three planes a KMD must implement

WDDM is not one protocol, it is three that share an object table:

1. **Memory plane** — the KMD describes memory *segments*; VidMm places
   allocations into them, evicts them under pressure, and asks the KMD to move
   bytes by building *paging buffers*. If GPU virtual addressing is supported,
   VidMm also owns the page tables and asks the KMD to write PTEs.
   → `04-memory.md`

2. **Execution plane** — VidSch decides which context runs on which engine, and
   hands the KMD DMA buffers to submit. Fences report completion back.
   Everything about preemption, priority and TDR lives here.
   → `05-scheduling.md`

3. **Display plane** — VidPn describes the topology of sources (desktop
   surfaces) and targets (connectors), and the flip DDIs put a specific
   allocation on screen at a specific vsync.
   → `06-display.md`

A paravirtualized driver can be shallow in plane 1 and 3 and still be correct,
but plane 2 is where the interesting design decisions are.

## The DMA buffer is the whole point

The single mechanism the entire model is built around:

- The UMD records commands into a **DMA buffer** — an opaque byte range whose
  format is entirely private to the vendor. Windows never parses it.
- Alongside it the UMD produces **DMA buffer private data**, also opaque, and
  an **allocation list** and **patch location list** naming which allocations
  the buffer touches and where their addresses appear inside it.
- `DxgkDdiRender` is called in the *submitting process's* context to translate
  the UMD's buffer into the final hardware buffer.
- `DxgkDdiPatch` (legacy physical addressing only) fixes up addresses once
  VidMm knows where the allocations actually landed.
- `DxgkDdiSubmitCommand` / `DxgkDdiSubmitCommandVirtual` hands it to the engine.

Because the DMA buffer format is opaque to Windows, **a paravirtualized driver
is free to define it as a serialized command stream for the host.** That is the
entire trick, and it is a supported, first-class use of the model — it is
exactly what `basicrender.sys` does with software rasterization, and what
GPU-PV does with VMBus.
