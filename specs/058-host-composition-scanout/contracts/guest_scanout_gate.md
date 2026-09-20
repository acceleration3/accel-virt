# Contract: what the guest does with the scanout bit (058)

The guest half of this feature, for the features that finish it. Nothing here
is built in 058's device-side work; it is written so 041, 056 and the task list
below agree on one story rather than three.

## The gate

`CapsetInfo::feature_dmabuf_scanout` is the only signal. The KMD reads the
capset once at `StartDevice`, as it already reads
`feature_host_visible_memory` (`src/core/memory/host_visible_segment.cpp`).

| Bit | What `DisplayScanout` does per source |
|---|---|
| absent | `RESOURCE_CREATE_2D`, `RESOURCE_ATTACH_BACKING`, `SET_SCANOUT`, `TRANSFER_TO_HOST_2D`, `RESOURCE_FLUSH` - exactly what it does today, unchanged, and feature 038's scenarios are its test |
| present, primary is guest-backed | `RESOURCE_CREATE_BLOB` (`GUEST`) over the primary's pages, `SET_SCANOUT_BLOB`, and `RESOURCE_FLUSH` per presented frame. No `TRANSFER_TO_HOST_2D` ever |
| present, primary is host-visible (041) | the same, over the blob 041's segment already made |

The decision is taken once, at `StartDevice`, and logged once (FR-006). It is
not re-taken per frame and not per source: a device either shows dma-bufs or it
does not.

## What changes on the guest side, and what does not

**Does not change.** `DxgkDdiSetVidPnSourceAddress` still records the address
and the handle in `DisplaySources` at `Irql::isr` and still sends nothing;
the adapter DPC is still what talks to the device; `SyntheticVsync` still
paces at the mode's refresh rate; `DxgkDdiPresent` still writes a
`PresentCommand` and still moves no pixels for a flip. No DDI slot is added and
none of the seven NULL flip-queue and multi-plane-overlay slots is filled
(FR-002).

**Changes.** `ScanoutResource` gains a second way to be backed - one blob per
source instead of one 2D resource - chosen at `configure()` from the bit and
never afterwards. `DisplayScanout::refresh` then sends `SET_SCANOUT_BLOB` where
it sent `SET_SCANOUT` and nothing where it sent `TRANSFER_TO_HOST_2D`.

**Is 041's.** Whether the primary's pages are guest RAM or the hostmem region.
The device takes either; only the second is "the desktop on the host GPU", and
only 041 can deliver it, because the runner has already established that the
guest's `D3DKMTLock2` view of a host-visible allocation is not the device's BAR
and that clients asking for one take the fallback instead.

**Is 056's.** Whether anything ever writes the primary. Today nothing does
(research §1).

## The vertical blank, when it comes (FR-005)

Not in this feature, and the shape it would take if it were:

- The device gains a periodic timer per shown head at the mode's refresh rate,
  and raises an interrupt the guest can tell from the configuration-changed one
  it already recognises (`Adapter::interrupt_routine`,
  `src/core/adapter/adapter.cpp:366`).
- `SyntheticVsync` becomes the source of last resort rather than the only
  source: with a device vblank arriving, its timer stops reporting, and it
  starts again the moment the device's stops - because DWM's frame pacing
  stopping is worse than its being slightly wrong.
- `InterruptNotification::report_vsync(target, address)` is unchanged; only who
  calls it moves.

None of that can be proven on Linux alone and none of it has a caller until the
desktop is actually flipping, which is why research §7 defers it with its
reason rather than leaving FR-005 looking done.
