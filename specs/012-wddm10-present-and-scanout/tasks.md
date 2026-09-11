---
description: "Task list for feature 012: WDDM 1.0 present and scanout"
---

# Tasks: WDDM 1.0 present and scanout

## Phase 1: Platform timer

- [X] T001 Contract rows; tests then `MockPlatform` timers (IRQL rules, leak check, `run_timers`)
- [X] T002 `KernelPlatform` timers; tests then `PeriodicTimer`

**Checkpoint**: commit.

## Phase 2: Present (US1)

- [X] T003 Records and km proofs; Linux layout tests
- [X] T004 Tests then `PresentCommand` (proto) and its protocol section
- [X] T005 Tests then `PresentRequest`

**Checkpoint**: commit.

## Phase 3: Scanout and vsync (US2)

- [X] T006 Tests then scanout state in `DisplaySources`, `InterruptNotification::report_vsync`
- [X] T007 Tests then `SyntheticVsync`; adapter DDIs, entry points, three slots

**Checkpoint**: commit.

## Phase 4: Scenarios

- [X] T008 Scenarios through the table; fuzz_present with corpus
- [X] T009 Docs; gate; merge; push
