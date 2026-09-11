---
description: "Task list for feature 028: QEMU device blob resources and host-visible memory"
---

# Tasks: QEMU device blob resources and host-visible memory

## Phase 1: Protocol

- [X] T001 Tests then `CreateBlobRequest`, `BlobRequest`, `GpuResponseWriter`; fuzz_blob_request with corpus

**Checkpoint**: commit.

## Phase 2: Host memory

- [X] T002 Tests then the `Host` extension and `MockHost`; `GuestPageMapping`, `HostMemoryBlock`, `HostmemSpace`

**Checkpoint**: commit.

## Phase 3: Blobs

- [X] T003 Tests then `BlobResource`, `BlobTable`, `ControlRequest`, `BlobCommands`; `VirtioGpuRvk::control`, `rvk_device_control`, `blob=on` refusal

**Checkpoint**: commit.

## Phase 4: QEMU

- [ ] T004 Glue: process_cmd routing, host operations, background hostmem; qtest blob suite; docs; gate; merge; push
