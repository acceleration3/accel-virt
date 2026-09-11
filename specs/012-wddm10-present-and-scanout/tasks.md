---
description: "Task list for feature 012: WDDM 1.0 present and scanout"
---

# Tasks: WDDM 1.0 present and scanout

## Phase 1: Platform timer

- [X] T001 Contract rows; tests then `MockPlatform` timers (IRQL rules, leak check, `run_timers`)
- [X] T002 `KernelPlatform` timers; tests then `PeriodicTimer`

**Checkpoint**: commit.

## Phase 2: Present (US1)

- [ ] T003 Records and km proofs; Linux layout tests
- [ ] T004 Tests then `PresentCommand` (proto) and its protocol section
- [ ] T005 Tests then `PresentRequest`

**Checkpoint**: commit.

## Phase 3: Scanout and vsync (US2)

- [ ] T006 Tests then `SourceScanout`, `DisplaySources`, `InterruptNotification::report_vsync`
- [ ] T007 Tests then `SyntheticVsync`; adapter DDIs, entry points, three slots

**Checkpoint**: commit.

## Phase 4: Scenarios

- [ ] T008 Scenarios through the table; fuzz_present with corpus
- [ ] T009 Docs; gate; merge; push
