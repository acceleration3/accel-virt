# Quickstart: power, restart and recovery

## L1 and L2, on Linux

```bash
ctest --test-dir build -R "engine_progress|context_loss|power_transition|ddi_scenarios"
```

## L5, in the guest

```bash
AV_GUEST_IMAGE=/path/to/windows.qcow2 tools/guest/run_guest_suite.py --power-cycles 50
```

adds, after the display scenarios:

- `suspend`: `guest-suspend-ram` through the agent, QMP `system_wakeup`, then
  the check stage and `rvktest.exe`. Reported skipped, with its reason, on a
  machine that does not offer S3.
- `hibernate`: hibernation enabled inside the overlay, `guest-suspend-disk`,
  the overlay booted again on the same QEMU configuration, then the same
  checks.
- `cycle`: `pnputil /disable-device` and `/enable-device` N times, the adapter
  started after each, and Driver Verifier's pool tracking read after the last.
- `upgrade`: a package with a higher version installed over the running one,
  with no reboot, and the adapter started on the new one.
- `tdr`: the executor told to stall one context past the timeout; the run
  passes when the other context keeps completing, event 4101 is in the System
  log, and the live kernel report reaches `diag/dumps/`.

## What proves it

SC-001 through SC-004 in `spec.md`. The stall scenario is the only one that
needs the executor's controls, which the runner sets in QEMU's environment.
