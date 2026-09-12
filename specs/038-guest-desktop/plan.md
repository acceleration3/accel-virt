# Implementation Plan: Real-guest desktop: DWM on rvkgpu at a chosen mode

**Branch**: `038-guest-desktop` | **Date**: 2026-09-12 | **Spec**: [spec.md](spec.md)

## Summary

Feature 037 left the driver installing, starting, committing a VidPn and passing
23 of 23 in-guest scenarios. What it did **not** leave is a picture: nothing in
the driver ever programs the host's scanout. `DisplaySources::set_scanout`
records the primary's address atomically and returns, and
`GpuRequestWriter` can create a 2D resource, attach backing and transfer to the
host - which the cursor uses - but has no `SET_SCANOUT` and no `RESOURCE_FLUSH`.
So this feature is mostly one missing path, plus the instruments that prove it:
the committed frame of every source reaching the host, verified from Linux by
QMP `screendump`.

The mode list is the second gap. `DisplayModeList` is eleven hard-coded
resolutions with a comment deferring to feature 027; the device knows its real
geometry and never gets asked.

## Decisions

- **The scanout is programmed at PASSIVE and refreshed from the DPC.**
  `DxgkDdiSetVidPnSourceAddress` is reachable at device IRQL, where a virtqueue
  submission may not block, so it keeps recording the address as it does now.
  `CommitVidPn` creates the resource and issues `SET_SCANOUT`; the DPC, which
  already runs to harvest both queues, notices an address the host has not been
  told about and issues `TRANSFER_TO_HOST_2D` plus `RESOURCE_FLUSH`. One frame
  of latency is M1's price, and it is the same structure that already works for
  the cursor.
- **Modes come from the device's display info, not from the host's EDID.**
  `GET_DISPLAY_INFO` answers each scanout's rectangle, which is what QEMU's own
  `-display` size and any later display-configuration change report. The EDID
  this driver hands `dxgkrnl` stays synthetic, built from the preferred mode, so
  "every EDID mode is offered and none that is not" (US2 acceptance 1) holds by
  construction rather than by parsing timing descriptors out of a block the host
  synthesized from the same number. `GET_EDID` stays unasked until something
  needs a real monitor's block.
- **Every display byte from the device is parsed by a class that owns the
  result.** `DisplayInfoResponse` validates the count, the rectangles and the
  enabled flags before anything reads them (FR-005, constitution V), with a
  hostile-input table and a fuzz target, exactly as the other readers have.
- **The output count comes from the device.** virtio-gpu's config carries
  `num_scanouts`; `DisplaySources` keeps a compile-time maximum for its arrays
  and a live count read at start, so US4's `max_outputs=2` needs no new array.
- **No new capability is reported.** Rotation, scaling, gamma and multi-plane
  overlay stay refused as they already are (FR-006, constitution VI); this
  feature adds tests that they are refused, not code that accepts them.
- **Every mismatch is reproduced at L2 first** (FR-007), as in 037: the L5 run
  says what disagreed, the harness reproduces it citing the `dxgkrnl.sys` or
  QEMU function it was read from, and the fix lands after the red test.

## Technical Context

**Language/Version**: C++23, freestanding (`-fno-exceptions -fno-rtti`, no STL,
no heap). Python 3 for the runner.

**Primary Dependencies**: none added. QEMU's stock virtio-gpu 2D path answers
`SET_SCANOUT`, `RESOURCE_FLUSH` and `GET_DISPLAY_INFO`; `virtio-gpu-rvk-pci`
inherits them.

**Testing**: L1 unit tests over `MockPlatform`, L2 DDI scenarios over the fake
`dxgkrnl`, L3 the fake virtio-gpu device, L5 the in-guest runner. `ctest` and
`AV_GUEST_IMAGE=... tools/gate.sh`.

**Target Platform**: Windows 11 guest on QEMU 11.1.1 with `-vga none`, which the
runner already passes.

**Performance Goals**: vertical blank within 5% of the committed mode's refresh
rate over 60 s (SC-003). No flush when nothing changed (spec edge case: DWM idle).

**Constraints**: the submission path never blocks, allocates or takes a mutex;
`SetVidPnSourceAddress` is reachable at device IRQL.

**Scale/Scope**: up to four outputs (the device reports how many), one primary
resource per output.

## Constitution Check

| Principle | How this feature satisfies it |
|---|---|
| I Specification precedes implementation | This plan and `tasks.md` precede the code; the spec's classes table is filled below. |
| II The core is portable; the shim is not | Everything new is in `src/core/display`, `src/proto/virtio_gpu` and `src/guest`; `src/km` gains nothing. |
| III Object-oriented, with state | Four new classes, each with state and one responsibility; the table is in the spec. |
| IV Test-first and exhaustive | `DisplayInfoResponse` reads device bytes, so it gets a hostile table and a fuzz target; every new branch is covered or marked. |
| V Everything from outside is hostile | The display-info response is length-checked and range-checked before use. |
| VI Feature levels are phases | No capability is reported that this feature does not implement and test. |
| VII IRQL and locking are typed | The new scanout work names its ceiling; the DPC path stays non-blocking. |
| VIII No speculative structure | `ScanoutResource` exists per source because the device has several scanouts; nothing is added for a second backend. |
| IX Reproducible and observable | Each scanout decision logs once through `LogBudget`, and the runner keeps its screendumps. |

## Project Structure

```text
src/core/display/
├── display_info_response.{hpp,cpp}   # the device's display info, validated
├── scanout_resource.{hpp,cpp}        # one host scanout resource per source
├── display_scanout.{hpp,cpp}         # the per-source collection
├── display_mode_list.{hpp,cpp}       # built from the display info now
└── display_sources.{hpp,cpp}         # a live output count

src/proto/virtio_gpu/
└── gpu_request_writer.{hpp,cpp}      # SET_SCANOUT, RESOURCE_FLUSH, GET_DISPLAY_INFO

src/guest/suite/
└── vsync_scenario.{hpp,cpp}          # the vertical-blank rate scenario

tools/guest/
└── run_guest_suite.py                # screendump capture and comparison, mode switching
```

**Structure Decision**: the display area of core gains the scanout path; the
wire writer gains three requests; the in-guest suite gains one scenario. No new
directory.

## Phases

- **Phase 1 (US1, P1)**: `SET_SCANOUT`/`RESOURCE_FLUSH` in the writer,
  `ScanoutResource`, `DisplayScanout`, the DPC refresh, and the runner's
  screendump capture and comparison. Done when the host's screendump changes
  when the guest's desktop does.
- **Phase 2 (US2, P1)**: `GET_DISPLAY_INFO`, `DisplayInfoResponse`, the mode
  list built from it, and the runner's mode-switch scenario.
- **Phase 3 (US3, P2)**: the vertical-blank rate scenario in `rvktest`.
- **Phase 4 (US4, P2)**: the live output count, two outputs in the runner, and
  display power.

## Complexity Tracking

No constitution violation to justify.
