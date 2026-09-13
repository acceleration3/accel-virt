# Phase 1 data model: what the probes own

## `ProbeResult` (object class)

One probe's answer.

| Field | Type | Meaning |
|---|---|---|
| `outcome_` | `Outcome` | `not_run`, `passed`, `failed` |
| `reason_` | `Status` | why it failed; `Status::ok` while it has not |
| `errno_` | `int32_t` | the host's own errno or `VkResult`, 0 when there is none |
| `measured_` | `uint64_t` | the bytes the probe carried, or the nanoseconds it took |

Transitions: `not_run` → `passed` or `failed`, once. `record_pass(measured)`
and `record_failure(reason, errno)` refuse a second answer with
`Status::invalid_state`, because a probe that ran twice is a probe whose result
nobody can name.

Behaviour: `passed()`, `ran()`, `reason()`, `errno_value()`, `measured()`, and
`describe(TextBuffer &)` for the one log line the device writes.

## `HostMemoryProbe` (object class) — US1

State: `Host &`, its `ProbeResult`, and, while running, the exported block and
the hostmem offset it placed it at.

`run()` (main loop):

1. Refuse with `not_supported` when `hostmem_size()` is 0 — a device with no
   region cannot offer the path, and the bit stays clear.
2. Ask the host for `probe_bytes` (1 MiB) of exported GPU memory.
3. Place it at hostmem offset 0.
4. Write `pattern` through the device's own pointer, read it back, and compare.
5. Remove it from hostmem and release it, whatever happened (`ScopeExit`).

Failure at any step is recorded with the host's errno and leaves the bit clear.
A readback mismatch is `Status::malformed`: the mapping existed and lied.

## `GuestImportProbe` (object class) — US2

State: `Host &`, its `ProbeResult`, and the guest window it locked while it
runs.

`run(uint64_t guest_address)` (main loop):

1. Map `probe_bytes` of guest RAM at `guest_address` through `map_guest`, and
   refuse with `not_found` when the host has nothing there or hands back a
   bounce buffer (`direct` false) — an import over a bounce buffer proves
   nothing.
2. Ask the host to import that window into the bound GPU.
3. Write a pattern into the window, ask the host to read it back through the
   import, and compare.
4. Release the import and the mapping.

`Status::not_supported` from the host (no `VK_EXT_external_memory_host`, or RAM
that is not shareable) is a clean *failed* result, not an error: the copy
fallback is what the guest then uses, and its throughput is what
`measured()` carries.

## `InterruptStressProbe` (object class) — US3

State: `Host &`, the fences raised, the acknowledgements seen, the worst
observed delay, and whether any interrupt arrived while the guest had its
interrupt disabled.

Unlike the memory probes this one does not run at realize: it is driven by the
guest's own run (the L5 suite) and by the L4 loopback stress, and it judges
what it saw:

- `raised(line)` (worker) — counts a fence interrupt the device injected.
- `acknowledged(count, nanoseconds)` (main loop) — what the guest reported.
- `disabled()` / `enabled()` (main loop) — the guest's mask window, from
  `DxgkDdiControlInterrupt`.
- `verdict()` — `failed` when any interrupt was injected while disabled, or
  when the last fence of the run was never acknowledged; `passed` otherwise,
  carrying the worst delay in `measured()`.

## `HostFeasibility` (system class) — US4

State: the three `ProbeResult`s, and whether it has decided.

`decide(uint64_t guest_probe_address)` (main loop, once at realize) runs the
two memory probes and remembers their results. `features()` returns the capset
feature mask: `feature_gpu_exported_memory` when the host-memory probe passed,
`feature_guest_memory_import` when the import probe passed, and
`feature_host_visible_memory` when a hostmem region exists at all (which is
what the device reports today). `interrupts()` hands out the stress probe for
the fence path to report into.

`DeviceCapset::describe` takes the mask from `HostFeasibility` instead of
deciding `feature_host_visible_memory` itself, so every bit in the capset has
exactly one origin.

## Validation rules

- No probe runs twice (`ProbeResult` enforces it).
- A probe never leaves an allocation, an import or a hostmem placement behind,
  on any path (`ScopeExit`, asserted by `MockHost`'s balance checks).
- A failed probe never fails realize: `decide()` returns nothing, and the
  device starts with fewer bits.
- Every bit the capset carries has a `passed()` result behind it
  (constitution VI), which an L1 test asserts for both values of each bit.
