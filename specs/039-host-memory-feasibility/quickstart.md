# Quickstart: running the feasibility probes

## What this proves

That the fast memory paths 041-044 are designed around exist *on this host*,
per GPU, and that a host where they do not exist falls back without crashing.

## Prerequisites

- The patched QEMU built: `cmake -S . -B build-qemu -DAV_QEMU=ON && cmake --build build-qemu`
- A host Vulkan device (`tools/host/list_gpus.py` lists them)
- KVM for the guest-side check: `/dev/kvm` readable by this user
- A kernel image for the minimal guest, by default the host's own
  `/boot/vmlinuz-*`

## The probes as the device runs them

```bash
tools/host/probe_host_memory.py --qemu build-qemu
```

One device per host GPU, realize only, no guest. It prints a JSON object per
GPU: the UUID, driver name and version, and for each probe the outcome, the
host error and what it measured. Exit status is 0 whenever every device
realized - a refused probe is an answer, not a failure.

```bash
tools/host/probe_host_memory.py --qemu build-qemu --guest-check --kernel /boot/vmlinuz-linux
```

Adds the KVM half of US1: a minimal Linux guest boots with the device, maps its
hostmem BAR, writes and reads back a pattern, and prints one line the tool
folds into the same JSON. Without `--kernel`, or without `/dev/kvm`, the tool
records the check as skipped with its reason.

## The interrupt stress

```bash
AV_GUEST_IMAGE=/path/to/windows.qcow2 tools/guest/run_guest_suite.py --stress-minutes 10
```

The guest raises fences as fast as the host sustains while the miniport
disables and re-enables its interrupt. The run fails if any interrupt arrives
while disabled or a final fence is never acknowledged; otherwise it prints the
rate and the worst delay.

## Expected outcomes on the development host

Recorded in `docs/kb/04-memory.md` after the first run: Intel Iris Xe (Mesa
ANV) and NVIDIA RTX 3070 Ti (proprietary). llvmpipe and AMD are recorded as not
measured, because this host has neither.

## Reading the answers back

Every bit the probes decide is visible to the guest in the rvk capset:

```bash
build-qemu/qemu-system-x86_64 ... -device virtio-gpu-rvk-pci,host-gpu=<uuid>
```

and in the device's log lines, one per probe, naming the outcome and the reason
a cleared bit is clear.
