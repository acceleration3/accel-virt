# Implementation Plan: In-guest integration suite

**Branch**: `035-guest-integration-suite` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`src/guest/` is the in-guest D3DKMT suite: portable scenario objects over a
`Kmt` interface, unit tested on Linux against `MockKmt`, and a Windows layer
(`src/guest/win32/`) translating `Kmt` into gdi32's D3DKMT functions, built
into `rvktest.exe` by a user-mode cross toolchain. `src/km/rvkgpu.inf` is the
driver package. `tools/guest/` boots a per-run overlay of a prepared image on
the patched QEMU, drives the guest over the QEMU guest agent, test-signs and
installs the package with Driver Verifier, runs `rvktest.exe` and collects the
verdict.

## Decisions

- **User-mode headers.** The kit gains the SDK's `um` headers that
  `windows.h` (lean) and `d3dkmthk.h` reach - a listed closure, not the
  directory - and its VERSION a subset line, so an older kit is refetched.
- **No C runtime.** `rvktest.exe` has its own entry point, `/NODEFAULTLIB`,
  and import libraries generated with `llvm-dlltool` from `.def` files naming
  exactly the functions it calls in gdi32, kernel32 and ntdll (`memset`,
  `memcpy`).
- **Kmt in core's types.** Handles are `uint32_t` (`D3DKMT_HANDLE`), results
  are `Status` with the NTSTATUS kept by the implementation for the report, so
  the portable code never sees a Windows header.
- **Waits.** Paging fences and context completion wait through an event with a
  timeout (`hAsyncEvent`, `WaitForSingleObject`), never an unbounded wait.
- **Report.** One line per step - `ok scenario.step` or
  `FAIL scenario.step status=N code=0xXXXXXXXX` - and a summary line; the exit
  code is the failure count. `TextBuffer` moves to `core/support` and gains
  hexadecimal, so the device and the guest share it.
- **Signing in the guest.** PowerShell's `New-SelfSignedCertificate`,
  `New-FileCatalog` and `Set-AuthenticodeSignature` - nothing from the WDK is
  needed on Linux or in the guest.
- **Snapshots.** "A snapshot before every install" is a qcow2 overlay per run,
  created before QEMU starts and deleted after it stops.
- **Testing the runner.** A fake `qemu-system-x86_64` (Python) parses the
  command line, serves a scripted guest agent on its socket and writes the
  debug log; a fake `qemu-img` creates the overlay file.

## Phases

1. Kit `um` closure; `TextBuffer` move and hex; user-mode toolchain; `rvktest`
   external project; image check.
2. `src/guest`: `KmtAdapter`, `PciIdentity`, `Kmt`, `MockKmt`, `GuestReport`,
   `AdapterSearch`, `GuestSession`, scenarios, `GuestSuite`; gates cover
   `src/guest`.
3. `src/guest/win32`: `GdiKmt`, `GuestProgram`, entry point; `rvkgpu.inf`.
4. `tools/guest`: agent client, runner, provisioning script; tests with fakes.
5. Docs, gate's guest configuration, merge, push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - scenario logic portable; Windows calls only in `src/guest/win32` |
| III | Pass - every class owns state; `Kmt` and `Scenario` each have more than one implementation |
| IV | Pass - `MockKmt` drives every branch; no byte decoding added, so no fuzz target |
| VIII | Pass - no capability reported; the suite observes what the driver already declares |
