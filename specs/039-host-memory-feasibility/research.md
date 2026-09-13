# Phase 0 research: host memory and interrupt feasibility

## What the host already says, without running a probe

Captured with `vulkaninfo` on the development host, 2026-09-13
(Vulkan instance 1.4.357):

| GPU | Driver | Type | Heaps | `VK_EXT_external_memory_host` | `VK_KHR_external_memory_fd` | `VK_EXT_external_memory_dma_buf` |
|---|---|---|---|---|---|---|
| Intel Iris Xe (ADL GT2) | Intel open-source Mesa 26.2.2 | integrated | one, 11736 MiB device-local | yes | yes | yes |
| NVIDIA RTX 3070 Ti Laptop | NVIDIA proprietary | discrete | 8192 MiB device-local, 11736 MiB host | yes | yes | yes |

No lavapipe is installed and there is no AMD GPU in this machine, so SC-001's
llvmpipe and AMD rows are recorded as *not measured* rather than guessed.

**What this does not answer.** Every extension above is a promise about the
Vulkan API, not about KVM. Whether an exported allocation `mmap`s at all,
whether the mapping survives `MAP_FIXED` into the hostmem reservation, and
whether a guest reading it faults, are host-driver behaviour that only a probe
finds out. That is the feature.

## Decision: the probes run at realize, in the device, behind `Host`

**Decision.** Each probe is a device class that calls new `Host` operations;
Vulkan and `mmap` live only in `src/device/qemu/virtio-gpu-rvk-host.c`, which
already includes `<vulkan/vulkan.h>` to enumerate GPUs for `host-gpu=`.

**Rationale.** Constitution II: the device core is portable and every host call
is behind the `Host` contract, which `MockHost` implements for L1. A probe
written in the glue would be untestable and would duplicate the capset logic.
Realize is also the only moment FR-003 allows: the bit must describe *this*
host, before any guest reads the capset.

**Alternatives considered.** A standalone spike binary outside the device
(rejected: it would prove something about a program that is not the device, and
FR-003 wants the device's own answer); extending the `rvk_executor` ABI so
render-server's Vulkan does the allocation (rejected for now: the executor is
not loaded until a context is created, and the capset must already be decided -
this stays open for 042 if the ABI grows memory operations).

## Decision: one megabyte, allocated and released inside realize

**Decision.** Each memory probe uses a single 1 MiB allocation, page aligned,
placed at hostmem offset 0, and releases it before realize returns.

**Rationale.** The spike's questions are about mechanism, not capacity: a
mapping that works for a megabyte works for a gigabyte or fails for a reason
the errno names. A megabyte is small enough not to disturb a device whose
hostmem region defaults to 0 (no region, probe not run) and fast enough to add
nothing measurable to realize. FR-004 makes cleanup part of the probe, not of
the caller.

**Alternatives considered.** The spec's US1 asks about sizes from 4 KiB to
1 GiB. Sweeping that range at realize would cost seconds and could exhaust a
small hostmem BAR, so the sweep belongs to the host tool, which can start a
device per size and read each verdict; the realize-time probe answers the one
question the capset needs.

## Decision: the guest half of US1 is a minimal Linux guest built in-tree

**Decision.** A tiny static init, built by the repo's own toolchain into a cpio
initramfs and booted with the host kernel under KVM, `mmap`s the device's
hostmem BAR through `/sys/bus/pci/devices/*/resource`, writes a pattern, reads
it back, and prints one machine-readable line. No distribution image, no
busybox (this host has none), no virtio driver: the check is about KVM mapping
the memslot, and PCI resource mapping through sysfs needs nothing else.

**Rationale.** The spec explicitly allows a minimal Linux guest and says the
question is the host's KVM and GPU driver behaviour, not Windows. The Windows
guest cannot answer it today: nothing in the KMD maps a blob yet (that is 041).
A static init written against raw syscalls fits the repo's freestanding style
and needs no libc.

**Alternatives considered.** Waiting for 041 so the Windows guest can map a
blob (rejected: 041 depends on this feature's answers); qtest only (rejected:
qtest has no KVM, so it cannot fault a memslot); a prebuilt cloud image
(rejected: `self_contained` forbids reaching outside the repository, and a
kernel image the host already has is not a repository dependency - the tool
names it and skips with a reason when it is absent).

## Decision: US3's masking comes from the driver we control

**Decision.** The stress run is the Windows guest suite raising fences as fast
as the host sustains while the miniport enables and disables its interrupt
through `DxgkDdiControlInterrupt`; the device counts what it raised, the guest
counts what it saw, and the probe fails if a final fence interrupt is ever
lost or one arrives while disabled.

**Rationale.** "The guest masks and unmasks vectors" is, for a WDDM miniport,
exactly `DxgkDdiControlInterrupt`/`ControlInterrupt2`, which this driver
already implements. Using it keeps the stress on the real path - irqfd, MSI-X,
the ISR - instead of a synthetic one, and the guest suite is already the L5
gate.

**Alternatives considered.** Masking MSI-X vectors from a Linux guest's sysfs
(rejected: it would test QEMU's masking, not ours); a device-only loop with no
guest (kept as the L4 stress, which catches lost wakeups but cannot observe
masking).

## Decision: two new capset bits, and what a cleared bit means

**Decision.** `CapsetInfo` gains `feature_gpu_exported_memory` (bit 2) and
`feature_guest_memory_import` (bit 4) beside the existing
`feature_host_visible_memory` (bit 1). Interrupt stress sets no bit: it is a
measurement of a path the device always uses, and its failure is a defect, not
a capability.

**Rationale.** A bit exists so a guest component can take a different path.
There are two such paths (042-044 map host GPU memory into the guest, or copy;
041 imports guest pages, or copies), so there are two bits. A third bit for
"interrupts work" would describe a path with no alternative.

**Alternatives considered.** One bit for "fast memory" (rejected: the two
mechanisms fail independently - a driver can export memory KVM maps while
refusing to import guest pages).

## Decision: the results are recorded where the design reads them

**Decision.** `tools/host/probe_host_memory.py` runs the device once per host
GPU, in each mode, and writes JSON; the answers for this host are written into
`docs/kb/04-memory.md` as a table naming driver, version, outcome, errno and
throughput.

**Rationale.** FR-002 asks for the knowledge base, and 041-044 read the
knowledge base. The JSON is what a second host adds a row from.

## Open question carried into the tasks

The throughput of the copy fallback (US2's second scenario) is measured on the
device side with the existing blob path; it is a number, not a gate, and the
task records it rather than asserting a threshold.
