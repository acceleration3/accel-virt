---
description: "Task list for feature 013: WDDM 1.0 cursor, and declining the legacy surface"
---

# Tasks: WDDM 1.0 cursor, and declining the legacy surface

## Phase 1: virtio-gpu requests

- [X] T001 Tests then `GpuRequestWriter` (proto); protocol section
- [X] T002 Fake virtqueue device reads requests and writes responses; tests then `GpuCommandQueue`
- [X] T003 Tests then `VirtioGpuDevice` send and harvest on both queues; DPC harvests the cursor queue

**Checkpoint**: commit.

## Phase 2: Cursor (US1)

- [ ] T004 Tests then `CursorImage`; fuzz_cursor_image with corpus
- [ ] T005 Tests then `HardwareCursor`

**Checkpoint**: commit.

## Phase 3: DDIs (US1, US2)

- [ ] T006 Records and km proofs; pointer capabilities
- [ ] T007 Tests then adapter pointer DDIs, the eight stubs, entry points, ten slots
- [ ] T008 Scenario through the table; docs; gate; merge; push
