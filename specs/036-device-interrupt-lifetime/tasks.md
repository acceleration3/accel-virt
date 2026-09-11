---
description: "Task list for feature 036: fence interrupts that follow the driver's lifetime"
---

# Tasks: QEMU device: fence interrupts that follow the driver's lifetime

Tests come first in every task and must fail before the code they cover exists.

## Phase 1: The status object

- [X] T001 [US1] [US3] Tests then `DriverStatus` in src/device/module/driver_status.{hpp,cpp}: a table over every status write (0, 1, 3, 11, 15, a repeat of each, `DRIVER_OK` set, cleared and set again), asserting the host's connect and disconnect counts, that a repeat asks the host nothing, that a refused connect is logged once with the status value that caused it, and invariants after every write
- [X] T002 [US1] [US3] `VirtioGpuRvk::status` forwards to `DriverStatus` and `create_context` answers `invalid_state` while the interrupts are down, in src/device/module/virtio_gpu_rvk.{hpp,cpp}; the status test in tests/unit/device/module/test_virtio_gpu_rvk.cpp inverts its "status 0 comes before the reset" case; tests/unit/device/module/test_device_module.cpp and the context tests set `DRIVER_OK` before creating a context
- [X] T003 [US3] Harness: `MockHost::last_log` in tests/harness/host/mock_host.{hpp,cpp} so a test asserts the refusal's text, with its assertion in tests/unit/harness/host/test_mock_host.cpp

**Checkpoint**: commit.

## Phase 2: The MSI-X sequences under qtest

- [ ] T004 [US1] tests/qemu/test_virtio_gpu_fences.py: MSI-X disabled and enabled again while `DRIVER_OK` stands - QEMU still running, and the next completion's interrupt arrives on its vector
- [ ] T005 [US1] tests/qemu/test_virtio_gpu_fences.py: 035's restart sequence - `DRIVER_OK`, a status write without it, MSI-X off and on, `DRIVER_OK` again - delivers a fence on its vector, and a fence raised while `DRIVER_OK` is clear is dropped with progress still readable from the fence page
- [ ] T006 [US2] tests/qemu/test_virtio_gpu_fences.py: a fence queue moved to another vector interrupts on the new one only, a queue left at `VIRTIO_MSI_NO_VECTOR` drops its fence, and MSI-X enabled only after `DRIVER_OK` still delivers
- [ ] T007 [US1] tests/qemu/test_virtio_gpu_fences.py: a sweep of MSI-X enable toggles leaves QEMU's open descriptors where they started, read from /proc/<pid>/fd

**Checkpoint**: commit.

## Phase 3: Loopback, docs and the live guest

- [ ] T008 [US1] tests/loopback/: `DRIVER_OK` cleared while a worker raises - the raise reaches no notifier, the context still tears down, and a later `DRIVER_OK` connects once
- [ ] T009 Docs: docs/testing.md (the MSI-X lifetime suite under "The device under qtest"), docs/roadmap.md status; `tools/gate.sh`; merge; push
- [ ] T010 [US1] The live run with `AV_GUEST_IMAGE`: the install stage passes where 035's run aborted, `--trace 'kvm_irqchip_*'` shows one route added per vector per connect, and 035's T016 closes

**Checkpoint**: commit.
