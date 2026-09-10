# 06 — Display: VidPn, modes, present, flip

The display half is where most of the 188 DDI slots live and where most of the
tedium is. It is also, for a paravirtual driver, the half you can make almost
entirely synthetic.

## VidPn

A **Video Present Network** is a graph:

- **Source** — a surface the OS renders the desktop into. One per logical
  display. `D3DDDI_VIDEO_PRESENT_SOURCE_ID`.
- **Target** — a physical output. `D3DDDI_VIDEO_PRESENT_TARGET_ID`. These are
  the "children" you reported from `DxgkDdiQueryChildRelations`.
- **Path** — a source bound to a target, with scaling, rotation, colour space.
- **Mode set** — per source and per target, the set of modes that are legal.

`dxgkrnl` owns the VidPn object and gives the KMD an *interface* to it. The
DDI names ending in `DXGKDDI_VIDPN*` in `d3dkmddi.h` are the functions the KMD
calls **on** the VidPn, not functions the KMD implements. What the KMD
implements is the smaller set:

| DDI | Contract |
|---|---|
| `DxgkDdiIsSupportedVidPn` | Given a proposed topology, say yes or no. |
| `DxgkDdiRecommendFunctionalVidPn` | Propose a topology yourself (boot/hotplug). |
| `DxgkDdiEnumVidPnCofuncModality` | For each path, fill in the mode sets that are simultaneously achievable. This is the hard one. |
| `DxgkDdiCommitVidPn` | Make it real. |
| `DxgkDdiUpdateActiveVidPnPresentPath` | A path's properties changed. |
| `DxgkDdiRecommendMonitorModes` | Add modes beyond what the EDID says. |
| `DxgkDdiSetVidPnSourceVisibility` | Turn a source on/off. |
| `DxgkDdiSetVidPnSourceAddress` | Point a source at an allocation — the flip. |
| `DxgkDdiSetTimingsFromVidPn` | Program the actual timings. |
| `DxgkDdiResetDisplayEngine` (3.2) | Reset just the display engine. |

For a virtual display, `EnumVidPnCofuncModality` is where implementations
usually go wrong. The rule: for every path where the source mode set or target
mode set is not pinned, you must populate it with every mode you can support
given what *is* pinned. Return an empty set and mode changes silently fail.

## Child devices and EDID

`DxgkDdiQueryChildRelations` reports one `DXGK_CHILD_DESCRIPTOR` per target,
with `ChildDeviceType = TypeVideoOutput` and
`ChildCapabilities.HpdAwareness = HpdAwarenessInterruptible` if you want
hotplug. `DxgkDdiQueryChildStatus` reports connected/disconnected.
`DxgkDdiQueryDeviceDescriptor` returns EDID bytes.

**A synthetic display should return a synthesized EDID.** Windows derives the
monitor name, native resolution, and colour capabilities from it, and several
things (HDR, colour management, DPI) behave badly without one. Generating a
128-byte EDID 1.3 block with a detailed timing descriptor for your preferred
mode is ~60 lines of code and saves a lot of debugging.

## Presenting

Three mechanisms, in increasing order of preference:

1. **`DxgkDdiPresent`** — build a DMA buffer that blits/flips. Called for the
   full-miniport path. `basicrender.sys` implements it as
   `WarpKMPresent` → `WARPKMCONTEXT::Present`, which builds a
   `WARPGPUCMDPRESENT` command object and queues it like any other work.
2. **`DxgkDdiSetVidPnSourceAddress`** — a true flip: point the scanout at a
   different allocation at the next vsync. Report completion with a
   `CRTC_VSYNC` interrupt carrying the flipped-to address.
3. **Multi-plane overlay** — `DxgkDdiCheckMultiPlaneOverlaySupport3` and
   `SetVidPnSourceAddressWithMultiPlaneOverlay3`. Lets DWM hand the hardware
   several planes and have it composite. Skip this initially; it is a large
   surface and DWM works fine without it.

`DxgkDdiControlInterrupt` / `ControlInterrupt2` / `ControlInterrupt3` is how
`dxgkrnl` asks you to enable or disable vsync interrupt reporting. A virtual
display must synthesize vsync — a timer at the mode's refresh rate calling
`DxgkCbNotifyInterrupt(CRTC_VSYNC)` — because DWM's entire frame pacing depends
on it. Without it the desktop composites at an erratic rate or not at all.

WDDM 2.2's `DxgkDdiCreatePeriodicFrameNotification` is the sanctioned way to
get a periodic callback for exactly this.

## Display-only drivers, for contrast

`DxgkInitializeDisplayOnlyDriver` + `KMDDOD_INITIALIZATION_DATA` gives you a
much smaller table: `DxgkDdiPresentDisplayOnly` receives a system-memory
bitmap and a dirty-rect list, and you copy it to the framebuffer. No contexts,
no DMA buffers, no GPU VA. This is what `basicdisplay.sys` does and what
`viogpudo` (the VirtIO display driver) does.

It is a legitimate first milestone — get a picture on screen with a KMDOD, then
throw it away and build the full miniport — but understand that **you cannot
grow a KMDOD into a render driver.** They are different entry points with
different init structures. Plan the throwaway deliberately or skip it.
