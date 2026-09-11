---
description: "Task list for feature 026: QEMU device skeleton, object model and threading"
---

# Tasks: QEMU device skeleton, object model and threading

## Phase 1: Host and binding

- [X] T001 Tests then `Host` and `MockHost` (threads, events, mutexes, main loop queue, catalog, violations, failure injection)
- [X] T002 Tests then `DeviceUuid`, `TextBuffer`, `GpuBinding`

**Checkpoint**: commit.

## Phase 2: Threading

- [ ] T003 Tests then `WorkItem`, `Worker`, `DeviceContext`, `DeviceContextTable`

**Checkpoint**: commit.

## Phase 3: The device and its C ABI

- [ ] T004 Tests then `RvkDevice`, `TableHost`, `DeviceInstance`, `DeviceModule`, `rvk_device.h` and entry points; `librvk_device.a` and `rvk_device.pc`

**Checkpoint**: commit.

## Phase 4: QEMU

- [ ] T005 C glue, meson patch, `tools/fetch_qemu.sh`, `tools/build_qemu.sh`, qtest smoke test in the gate
- [ ] T006 `tools/host/list_gpus.py`; docs, kb corrections, constitution amendment; gate; merge; push
