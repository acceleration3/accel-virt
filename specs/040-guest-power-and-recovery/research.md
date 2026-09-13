# Phase 0 research: power, restart and recovery

## What the timeout path actually does with `QueryEngineStatus`

Read from `dxgmms2.sys`, not assumed.

`VidSchiCheckNodeTimeout` (@1400c9b50) is the periodic check:

1. It takes the node's elapsed time as a difference of checkpoints multiplied by
   `KeQueryTimeIncrement()`, and compares it against `g_TdrConfig + 4`
   (`TdrDelay`) times 10,000,000 - seconds in 100 ns units, the familiar two.
2. **Only then, and only for a device that allows long-running packets**
   (`VidSchiRunningDeviceAllowsLongRunningPackets` @14004505c, which wants every
   queued packet accounted for and none in flight), does it call
   `ADAPTER_RENDER::DdiQueryEngineStatus` (dxgkrnl.sys @140027dc0).
3. **If that call succeeds and the answer has bit 0 set, the check returns
   zero - no timeout this round.** Otherwise it captures the running process,
   marks the node timed out, and goes on to `VidSchiReportHwHang`.

**So "responsive" is a deferral, asked once per check, and the driver we have
today answers it unconditionally.** A genuinely hung host would be deferred
forever: the guest would sit there, no TDR, no recovery, no event 4101 - the
opposite of what a remoting driver needs. That is the defect FR-002 names, and
it is the reason `EngineProgress` exists: the answer must be "yes" only while
the node's own progress is moving, and "no" the moment it is not.

**Decision.** `QueryEngineStatus` answers responsive only when the node has
moved since the last time it was asked, or has nothing queued. Anything else
answers with the bit clear and lets dxgkrnl proceed to a reset.

**Alternatives considered.** Reporting responsive while the *host* still
answers a ping (rejected: it puts a round trip on a DISPATCH-level path inside
dxgkrnl's own timeout); never answering responsive at all (rejected: a slow but
progressing host would be reset for being slow, which is SC-003's second half).

## What both DDIs require of the driver

`DdiQueryEngineStatus` and `DdiResetEngine` (@14006e0a4) are thin wrappers with
the same shape: `DXGADAPTER::AcquireDdiSync`, a counter, the call, and then
`KeGetCurrentIrql()` compared with the IRQL on entry. A mismatch logs a
watchdog entry of class 0x113 - the display bugcheck family. **A driver that
returns at a different IRQL than it was called at is a bugcheck waiting for a
checked build**, which is worth knowing for a reset path that is tempted to
drop a lock the wrong way.

## What a power transition leaves behind

`Adapter::power_down` today stops the virtio device and destroys it;
`power_up` constructs and starts it again, and tells the cursor and the scanout
to forget the device they had. What it does not do is put back what the guest
still believes in: the primary that was scanning out, and the aperture pages of
allocations that VidMm never released across the transition.

**Decision.** `PowerTransition` remembers what was running when power went away
- whether the device was started, and which sources had a committed mode and a
primary - and puts exactly that back on the way up. Nothing else is restored:
contexts are the guest's, and what cannot be restored is reported lost.

**Rationale.** D3 is not StopDevice. dxgkrnl keeps every allocation, VidPn and
context across it; the driver's own device object is what goes away, so the
driver's own device object is what comes back.

## What S4 does that S3 does not

For the driver, nothing: both arrive as `SetPowerState` toward D3 and back to
D0. What differs is the host - S4's resume is a fresh QEMU with a fresh device,
so the PCI configuration, the BAR mappings and the host GPU behind them are all
new, and the capset may have changed with them.

**Decision.** The resume path re-reads the device's configuration rather than
trusting what it had, and a device whose identity changed (a different host GPU
UUID, an older protocol version) is refused with the contexts that depended on
it reported lost - the desktop keeps working, because scanout does not depend on
the executor (FR-005).

## Injecting a stall without a rebuild

**Decision.** The echo executor (`tests/qemu/echo_executor.cpp`) gains two
controls read from its environment at load: a stall in milliseconds and a
fault, each naming the context they apply to. The runner sets them in the QEMU
process's environment for the scenario that needs them, so the guest can
produce a real TDR against a real host stall with nothing rebuilt (FR-006).

**Alternatives considered.** A control command from the guest (rejected: it
would put test-only commands in the protocol); a second executor library
(rejected: two things to keep in step).

## Open questions carried into the tasks

- Whether S3 is offered at all by this QEMU configuration is a property of the
  machine type; the scenario reports it skipped rather than failing when the
  guest agent refuses `guest-suspend-ram`.
- 50 disable/enable cycles is what SC-002 asks for; the runner does them in one
  boot, and Driver Verifier's pool tracking is read after the last one.
