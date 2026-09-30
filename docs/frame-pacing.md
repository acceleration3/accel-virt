# Frame pacing

Why a frame in the guest came late: a DWM composition, a Windows.Graphics.Capture
(WGC) frame, a present. `tools/frame_pacing.py` records everything that decides
it in one WPR trace and accounts for each DWM frame from it, without the guest.

```bash
tools/frame_pacing.py run                                 # WGC capture of the monitor over a D3D11 window, 8 s
tools/frame_pacing.py run --workload window --seconds 5   # the window itself, GDI-painted
tools/frame_pacing.py run --workload none --seconds 5     # whatever the guest is doing now
tools/frame_pacing.py record --name before                # record only
tools/frame_pacing.py analyze before --perfetto before.json
```

The guest must be held (`run_guest_suite.py --keep`, or the user's VM with its
agent). Recordings, the guest's event schemas and fetched symbols stay in
`build-perfetto/`.

## What is recorded

One WPR session (the profile is written by the tool, into the recording's
folder):

| Source | What | Why |
|---|---|---|
| kernel | context switches and readied threads, with stacks | when each thread ran, waited for a CPU, or blocked, and on what |
| kernel | CPU samples every 250 µs (`--sample-us`), with stacks | what code the time went to |
| kernel | DPCs and ISRs, with start and end | time interrupts took from the thread they landed on |
| kernel | processes, threads, images; WPR's PDB ids | names for all of the above |
| `Microsoft-Windows-DxgKrnl` | Base, Profiler, Present (0x8000003) | vblanks (VSyncDPC's QPC time), every dxgkrnl call's entry and exit, packets, presents |
| `Microsoft-Windows-Dwm-Core` | Scheduling, frame rate (0xC8) | DWM's frames, their stages, the reasons it schedules composition |
| `Microsoft-Windows-Win32k` | Updates (0x1000) | the flip manager: capture presents and buffer availability |

DxgKrnl and Win32k log from raised IRQL; their buffers must be non-paged or
the events are dropped silently. The workload is `tools/guest/probes/wgc_probe.exe`
run in the user's session (a WGC capture needs one); it reports each frame it
got, and its paints and frames call `D3DKMTGetScanLine`, which the Profiler
keyword logs with the calling thread, as markers.

Events are decoded by name with the schema the guest's own TDH gives
(`tools/guest/probes/etw_schema.exe`, cached per Windows build), so any field of
any event of those providers can be used without hard-coding its layout.

## Reading the report

- **trace** and **probe**: the span, the vblank period, lost buffers; what the
  capture application saw.
- **DWM**: its render thread, frames, and the capture flip manager's presents
  and skips.
- **rule**: how many skips the capture rule below explains, and how many
  predicted skips were captured after all. Every skip explained means the rest
  of the report is about why DWM reached its capture check late or early.
- **vblank → capture check**: the phase of the check in normal frames, in the
  frames a skip was measured from, and in the skipped ones.
- **the render thread**: from the vblank to the capture check, a frame's time
  running (less DPCs and ISRs on its CPU), waiting for a CPU, blocked (by wait
  reason), and in each dxgkrnl call, normal frames beside the frames a skip was
  measured from. Then its CPU samples by module, by function, and with callees
  included, ranked by how much more the slow frames spent there. The trace's
  own stack walks are counted separately: they are the recording's cost.
- **each skip**: when, the elapsed time the rule saw, and the two phases.

`--perfetto` writes the same trace for ui.perfetto.dev: one row per CPU with
the thread it ran and one with its DPCs and ISRs; the render thread's states,
frames (with phase, elapsed and prediction), stages and samples (with stacks);
and the vblanks.

## The WGC capture rule

A capture target renders in a DWM frame only when
`COffScreenRenderTarget::IsRenderTime` (dwmcore) allows it: at least
`MinUpdateInterval` of QPC time since the capture last started rendering
(`COffScreenRenderTarget::RenderTargetDirty` stamps it). The default is 16 ms
(`(QPC frequency << 4) / 1000` in the constructor), set per session by
`GraphicsCaptureSession.MinUpdateInterval` where Windows has it. Otherwise DWM
schedules a composition pass for reason DelayedRendering (bit 0x1 of
`SCHEDULED_COMPOSITION_REASON`) and the capture skips that vblank.

At 60 Hz a vblank is 16.67 ms, so the slack is 0.67 ms: a frame that reaches
its capture check more than that earlier after its vblank than the last
captured frame did is skipped. Every WGC drop measured so far is this rule
(98 of 98 across three captures, 2026-09-30), so a capture's frame rate is a
measure of how steady DWM's frame phase is. The tool times the check at the
legacy swapchain's buffer flip (`LEGACYSWAPCHAIN_BUFFERFLIP`), a few
microseconds before `IsRenderTime` reads the clock; a skip at 16.0x ms is that
difference.

## What it has found

- **2026-09-30, fixed** (8c322b0f, 147f657b): DPCs of 0.6 to 1.8 ms. The driver
  logged to QEMU's isa-debugcon a byte per VM exit, about a millisecond a line,
  at DISPATCH_LEVEL - and a race in `ExecutionNode::release_overdue` produced
  twelve lines a second under a capture. DWM, interrupted, reached its capture
  check late, and the next frame skipped: 55 to 56 captures a second. After:
  no DPC of 0.5 ms or longer, 58 to 59.6.
- **The rest**, measured with this tool: in the frames a skip is measured from,
  DWM's render thread runs about 2 ms instead of 0.7 - on the CPU the whole
  time, not waiting for one, not blocked, not interrupted. The extra is DWM's
  own composition (`CLegacyRenderTarget::Render`, `CComposition::PreRender`,
  `CComposition::CleanTrees`, Direct2D's `EndDraw`) and `memmove`; this
  driver's user-mode and kernel-mode code grow by a few tens of microseconds.

## Files

| | |
|---|---|
| `tools/frame_pacing.py` | record, analyze, run |
| `tools/etw/etl_file.py` | ETL records with their processor (struct-based, no dependencies) |
| `tools/etw/manifest.py` | manifest events decoded by the guest's TDH schema |
| `tools/etw/kernel.py` | the kernel's scheduling, samples, interrupts, processes, images and PDB ids |
| `tools/etw/threads.py` | one thread's states over time, and an account of an interval |
| `tools/etw/dwm.py` | DWM's frames, the capture rule, the groups the report compares |
| `tools/etw/symbols.py` | names from this repository's PDBs and Microsoft's symbol server |
| `tools/etw/report.py`, `tools/etw/perfetto.py` | the report and the Perfetto export |
| `tools/guest/probes/` | `wgc_probe.cpp`, `etw_schema.c` and their build (llvm-mingw) |
| `tests/tools/test_frame_pacing.py` | the reading and the analysis, on bytes and events built in the test |
