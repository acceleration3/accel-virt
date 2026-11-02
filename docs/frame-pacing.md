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
- **2026-09-30, fixed** (ecf52db4): in the frames a skip is measured from,
  DWM's render thread ran about 2 ms instead of 0.7 - on the CPU the whole
  time, every one of DWM's own functions slower alike. The host was moving
  the vCPU threads between the 9950X3D's two CCDs, and a vCPU resumed on a
  cold cache. Kept on one CCD, a normal frame takes 0.4 ms of CPU and the
  capture runs at 59.6 to 60.0 a second; `run_guest_suite.py` confines the
  vCPUs to one host cache domain (`tools/guest/cache_domains.py`).
- **2026-09-30, fixed** (dd4abbe5): each unmap of DWM's large dynamic buffers
  copied the whole buffer into the command stream, 89 ms of `memmove` a
  second; the written pages alone now go (write watch), 12 ms.
- **2026-10-15, fixed**: a capture of the 2560x1440 monitor on an Intel Iris Xe
  (ADL GT2) host at 51 frames a second (`remotewin-wgcbench`, D3D11 window), 59
  on the NVIDIA GPU of the same host. With MinUpdateInterval 0 the rule above
  skips nothing; a skip is DWM's `RENDERTARGET_RENDERSKIPPED`, in 35 of 51 with
  the previous capture's present posted and not yet complete. A capture's post
  to its completion took 5.3 ms at the median (2.2 on the NVIDIA GPU), and QEMU
  held 26 to 28% of the Xe's render engine against 9 to 11% for a window
  capture: every texture a guest shares - the capture's frame pool,
  DirectComposition's surfaces - was laid out linear, and on the Xe a draw into
  a linear image takes three and a half times as long as into the same image
  tiled, and one from it six times (`tools/host/shared_image_bench.c`). Shared
  textures are laid out in a tiling of the GPU's own now, linear only where
  another API reads them as rows (docs/external-export.md §6): 16 to 19% of the
  render engine, and 54 to 57 captures a second (58.4 to 59.3 of a GDI window,
  57.7 before). What is left is DWM's render thread, which renders the whole
  monitor into the capture every frame and is on the CPU three quarters of the
  time on that host in its power-saver profile (cores at 0.7 to 0.9 GHz), and
  the host desktop's own 20 to 29% of the same render engine.

- **2026-10-31**: a 2560x1440 Direct3D 9 window with
  `SupportSoftwareDeviceBitmaps` reported. DWM's render thread ran 5.6 ms a
  frame instead of 0.65, in `memcmp` and `memmove` under
  `D3d11Device::refresh_guest_surface`, and its capture check moved from 0.6 ms
  after the vblank to 5.8 (p90 8.1, max 79). Present to capture was 31 to 42 ms
  at the median and 59 to 67 at p95, against 30 to 31 and 34 to 35 without the
  bit, on a host that was busy. Lending the surface's pages to the host
  (`D3d11Device::open_cross_adapter`) takes that read out of the compose: with
  the bit, 59 captures a second of a GDI monitor (36 before), no read of the
  pages at all, and `d3d9_dirty_present` never more than a frame behind at a
  present every 4 ms (55 before), none torn.

- **2026-11-01**: frames on screen, through a mirror on a 360 Hz output, measured
  per frame with a 60 Hz source. Only 64% stayed exactly six refreshes (5, 6 or 7,
  13.9 to 19.4 ms, however even their commits), with hitches of 25 to 60 ms in
  about 1%. Four causes.
  - remotewin's `DisplayLock` brought commits down until 5% missed the compositor's
    latch, so it parked them on the edge and a millisecond of wander moved a frame
    by a whole refresh. Commits now stay a margin past the latch, which the lock
    finds by sweeping the phase once.
  - Each retiming (`VblankSchedule::align`) can make the guest's DWM drop frames
    for about 200 ms, and the lock retimed about once a second: 20 of 21 capture
    gaps followed one, most within 40 ms. Why DWM reacts so is not known. Settled,
    the lock no longer asks.
  - The compositor states its mode's refresh, 359.979 Hz on this monitor, which
    really runs at 359.975, and a millihertz cannot hold a phase anyway: rounding
    leaves up to 8 parts in a million, a retiming every 100 s. The lock measures
    the rate from its present stamps and the timing carries microhertz, and a
    relative shift rather than a moment on the host's clock, so it cannot be wrong
    about where the guest's blanks are.
  - One 35 to 60 ms gap about every 10 s was the disk. With `io='native'`, QEMU's
    main loop submits I/O itself, and on btrfs a submit can block; the main loop
    holds the BQL and runs slirp, so the vCPUs and the mirror's frames stopped
    with it. Its thread slept in `folio_wait_bit_common` for 20 to 88 ms, often 31,
    with no accel-virt function on its stack and no major fault. With
    `io='threads'`, 66 blocks (307 ms) in 60 s became 3 (3.6 ms).

  Measured on the 360 Hz output with the lock's first fixes, for 150 s: 97.5 to
  99.7% exactly six refreshes per 10 s, composed to presented 9.96 ms median and
  10.04 p95, with the disk's gaps still in. The final build has run only on a
  headless KWin at 60 Hz, on the same GPU with dma-bufs and explicit sync, for
  180 s: 0 of 8651 gaps off, a sweep and one correction and then no timing at all,
  composed to presented 18.74 ms (p5 18.67, p99 18.83), and the main loop blocked
  over 5 ms not once.

  The lock's latch comes only from its sweep. It once followed what commits were
  seen to take, and a compositor sometimes catches a commit past its latch, so
  each such frame walked the next commits later, to where they missed: to-screen
  fell from 12 ms to 3 and composed to presented rose to 26 ms, with every frame
  still evenly spaced. Spacing alone does not show this.

  Reading a frame log: KWin discards the feedback of a surface it is not compositing,
  so a log that stops, or `presented 0`, means a covered window or displays in DPMS
  off (`kscreen-doctor --dpms show`) before it means a bug. `kwin_wayland --virtual
  --socket lat-kwin` with `WAYLAND_DISPLAY=lat-kwin` for the mirror measures all of
  this with no display, and shows the user nothing.

- **2026-11-01, open**: a capture of the monitor sometimes takes a whole refresh more,
  and stays that way until the guest reboots: present to arrival 48 ms against 31.7,
  arrival after the capture's stamp by 4 ms rather than before it by 13, and a mirror
  logging `composed` to `taken` of about 20 ms rather than 2. A capture of a window
  stays at 30 ms throughout. It was first seen with `AllocationKind::surface` mapped
  Cached, and went on removing that, so the mapping stayed out; but it came back on a
  driver without it, after five idle hours, so that is not the whole cause. Not closing
  apps, not a power cycle of the guest's display, and not the host's load. Judge this
  by present to arrival: `composed` is the capture's stamp less a period, an assumption.

- **2026-11-01**: those pages stay write-combined, and what reads them uses streaming
  loads (`GuestSurfaceScan`): 80 MB/s through `memcmp`, and 642 to 11 ms a frame for a
  read-only `GetDC` of 2560x1440.

- **2026-11-01**: the guest's refresh is the lever on input delay. Input to capture in
  the guest, real input, full-screen Direct3D 9 (`latprobe32`, `LATFULL=1 LATCURSOR=9`,
  no `LATQUIET`), p50 / p95 / p99 in ms: 60 Hz 45 to 48 / 53 to 56 / 55 to 84; 120 Hz 24
  / 28 / 28 to 33; 180 Hz 17 / 20 / 21 to 24; 240 Hz 13 / 16 to 17 / 19 to 21. Windows
  said 60 Hz throughout in these runs, as `DisplayMode` offered nothing else, while
  `DisplayLock` aimed the blanks at about 180 Hz in whole periods of the output (360 Hz
  gave 180, 240 and below the output's own rate). Windows is told the real rate now: the
  device's `refresh` property is the one number the mode list and the blanks run at, the
  manager sets it (the monitor's own rate by default), and `DisplayLock` shifts the
  blanks and never changes their rate. The costs, at 180 Hz:
  - A frame every 5.56 ms reached the mirror (176 a second), but with a full-screen
    probe drawing every blank the host GPU was 92% busy and QEMU took 1.66 cores.
  - An application that paces itself to 60 fps lands on a finer grid: 14 to 23 ms
    between frames at the mirror against 16.66 to 17.54. Better latency, less even.
  - An application counting blanks as sixtieths of a second runs fast.
  - One of two 180 Hz rounds fell to 60 to 140 fps for its 8 s, unexplained.

  The lock said the rate alone before sweeping: a sweep over a guest changing rate
  found a false latch and needed a second sweep. It no longer says a rate at all, so
  nothing changes rate under a sweep except the host's own change of `refresh`, which
  starts the output's sweep over. With the guest's rate the device's, the lock is a
  lock only where the guest's period is the whole number of the output's periods
  (within 50 parts per million): 60 on a 60, 180 or 360 Hz output, 144 on a 144 or
  288 Hz one. A 144 Hz guest on a 240 Hz output, or 60 on 59.94, has commits that slide
  round the output's period, and nothing is asked for. A guest rate that is a
  millihertz off the output's real one, which the manager can only state to the
  millihertz, drifts the commits by a millisecond in two minutes at 9 parts in a million
  and is put back by one timing each time, about every ninety seconds. The final build
  has not run on a real 360 Hz output.

- **Open, not this work's**: for under a second after a process holding large shared
  textures exits, a new frame pool fails with `DXGI_ERROR_DEVICE_REMOVED` or
  `E_OUTOFMEMORY`. Driver 0.35.383 does it too. Shared textures are pinned in the
  aperture, which is the likely reason; not established.

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
