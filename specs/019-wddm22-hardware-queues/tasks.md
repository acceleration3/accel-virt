---
description: "Task list for feature 019: WDDM 2.2 hardware contexts and queues"
---

# Tasks: WDDM 2.2 hardware contexts and queues

## Phase 1: WDDM 2.2

- [X] T001 Harness: the display caps extension query; tests then WDDM 2.2 and `DISPLAY_DRIVERCAPS_EXTENSION`

**Checkpoint**: commit.

## Phase 2: Hardware queue model

- [X] T002 Records and km proofs; `FakeHwQueue`
- [X] T003 Tests then `HwQueue`, `HwQueueTable`, `HwQueueSubmissions`, MONITORED_FENCE_SIGNALED, HwQueueSupported contexts; CreateHwQueue, DestroyHwQueue, SubmitCommandToHwQueue, ResetHwEngine; entry points; slots

**Checkpoint**: commit.

## Phase 3: Scenario

- [X] T004 Scenario through the table; docs; gate; merge; push
