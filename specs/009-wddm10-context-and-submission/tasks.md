---
description: "Task list for feature 009: WDDM 1.0 contexts and packet submission"
---

# Tasks: WDDM 1.0 contexts and packet submission

## Phase 1: Records and callbacks

- [X] T001 DDI records and km proofs; Linux layout tests
- [X] T002 Tests then `DxgkrnlInterface::synchronize_execution`, `notify_interrupt`, `notify_dpc`; fake adapter callbacks

## Phase 2: Contexts (US1)

- [X] T003 Tests then `Context`, `ContextTable`; adapter CreateContext/DestroyContext, entry points, slots

**Checkpoint**: commit.

## Phase 3: Render and Patch (US2)

- [X] T004 Tests then `RenderRequest` (hostile table)
- [X] T005 Tests then `PatchRequest` (hostile table); adapter Render/Patch, entry points, slots; `fuzz_submission` with corpus

**Checkpoint**: commit.

## Phase 4: Execution and paging (US3, US4)

- [ ] T006 Tests then `InterruptNotification`, `AvSynchronizedNotify`
- [ ] T007 Tests then `ExecutionNode`; adapter SubmitCommand, PreemptCommand, QueryCurrentFence, DPC drain
- [ ] T008 Tests then `PagingRequest`; adapter BuildPagingBuffer

**Checkpoint**: commit.

## Phase 5: Harness and scenarios

- [ ] T009 `FakeVidSch` and tests; scenarios: the pipeline end to end, preempt, render core rule now missing ResetFromTimeout
- [ ] T010 Docs (`docs/ddi-sequences.md` submission and paging, `docs/testing.md`); gate; merge; push
