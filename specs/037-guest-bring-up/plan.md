# Implementation Plan: Real-guest bring-up: the in-guest suite passes

**Branch**: `037-guest-bring-up` | **Date**: 2026-09-12 | **Spec**: [spec.md](spec.md)

## Summary

The driver installs and loads on the real Windows, and dxgkrnl then refuses the
adapter. This feature is a diagnosis loop, not a body of new code: make the
driver say where it stops, read `dxgkrnl.sys` for what it wanted, reproduce the
disagreement in the L2 harness, fix it in core, and repeat until three
consecutive runs end with verdict 0.

The one thing known before the loop starts is that the driver is silent. `enter`
logs `rvkgpu: DriverEntry` and then registers the DDI table without logging the
status; `Miniport::add_device` and `Adapter::start_device` log nothing on any
path. So the live run's two `DriverEntry` lines and nothing after them are
equally consistent with success and with a refused registration. Instrumentation
comes first because every later step reads that log.

## Decisions

- **The log is the instrument.** `DriverEntry`, the registration status,
  `AddDevice`, `StartDevice` and each capability query log once, in order, with
  the value that decided them (FR-003, US1 acceptance 2). A refusal logs the
  status; a success logs the step.
- **Rate-bounded, because the ISR and DPC log too.** `LogBudget` owns a count
  and a window and answers whether a line may be written, so a fault storm at
  DISPATCH cannot log unboundedly (spec edge case). The DPC's existing refusal
  lines go through it.
- **No new capability.** Nothing this feature learns is reported until the
  in-guest run has exercised it (FR-005, constitution VI).
- **Every fix is an L2 scenario first.** A disagreement Windows exposes is
  reproduced in `tests/harness/dxgkrnl/` with the `dxgkrnl.sys` function it was
  read from cited in the scenario, and the fix lands after it (FR-001, US3).
- **Image rules are gate rules.** Anything the kernel checks before
  `DriverEntry` that the build got wrong becomes a `driver_image` rule citing
  the `ntoskrnl` function, as `TERMINAL_SERVER_AWARE` did (FR-002).
- **The root cause is read, not guessed.** `StartAdapter_AddAdapterFailed` with
  `STATUS_OBJECT_NAME_NOT_FOUND` is being traced from `DpiFdoStartAdapter`
  through `DXGGLOBAL::CreateAdapter` in `re/ghidra-proj`; the fix cites the call
  that produced it.

## Phases

1. Instrumentation: `LogBudget`, the registration status, the DDI step lines;
   an L1 test per line over `MockPlatform`. Re-run the guest and read where it
   stops.
2. The adapter-start defect: reproduce at L2 from what the log and the
   `dxgkrnl` read say, fix in core, re-run.
3. Whatever the started adapter exposes next, the same way, until
   `rvktest.exe`'s four scenarios pass under Driver Verifier.
4. Three consecutive clean runs; `tools/gate.sh` with `AV_GUEST_IMAGE` set
   becomes the merge gate (FR-006); docs; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - the log lines are decisions in core; the shim only writes bytes |
| III | Pass - `LogBudget` owns its count and window; no new free function |
| IV | Pass - each log line is asserted by an L1 test over the mock, each fix by an L2 scenario that fails before it |
| VI | Pass - no capability added; this feature only makes existing ones true |
| VII | Pass - the DISPATCH-level paths log through the budget, which does not allocate |
| IX | Pass - this feature is the one that makes the driver observable |
