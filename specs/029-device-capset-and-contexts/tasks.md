---
description: "Task list for feature 029: rvk capset and context lifetime"
---

# Tasks: QEMU device: rvk capset and context lifetime

## Phase 1: Protocol

- [X] T001 Tests then `ContextRequest`, `GpuResponse` capset answers; fuzz_context_request with corpus

**Checkpoint**: commit.

## Phase 2: Capset and bindings

- [ ] T002 Tests then `Host::gpu_identity` (C ABI, TableHost, MockHost, GpuBinding), `DeviceCapset`, blob pins, `ContextBinding`

**Checkpoint**: commit.

## Phase 3: Commands

- [ ] T003 Tests then `ContextCommands`, `VirtioGpuRvk` dispatch, `BlobCommands::answer`, `rvk_device_capsets`

**Checkpoint**: commit.

## Phase 4: QEMU

- [ ] T004 Glue: CONTEXT_INIT and num_capsets, identity catalog, larger responses; qtest context suite; docs; gate; merge; push
