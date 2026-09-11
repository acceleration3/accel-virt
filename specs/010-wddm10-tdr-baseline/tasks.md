---
description: "Task list for feature 010: WDDM 1.0 timeout detection and recovery"
---

# Tasks: WDDM 1.0 timeout detection and recovery

## Phase 1: Node

- [X] T001 Tests then `ExecutionNode::reset`, `restart`, `last_submitted`, `progress_time`, refusal while stopped

## Phase 2: DDIs and report

- [X] T002 Tests then `Adapter::reset_from_timeout`, `restart_from_timeout`, entry points, slots 32 and 33
- [X] T003 Tests then `DebugReport` format 2 with node progress

**Checkpoint**: commit.

## Phase 3: Scenarios

- [X] T004 `FakeVidSch::reset` / `restart`; scenarios: time out and recover twice, reset before start, render core rule now missing ControlInterrupt
- [X] T005 Docs (`docs/ddi-sequences.md` TDR, `docs/testing.md`); gate; merge; push
