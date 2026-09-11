---
description: "Task list for feature 015: WDDM 1.2 (Windows 8) additions"
---

# Tasks: WDDM 1.2 (Windows 8) additions

## Phase 1: Records

- [X] T001 Records, km proofs, Linux layout tests

## Phase 2: Engine recovery (US1)

- [X] T002 Tests then `ExecutionNode::abort_pending`; QueryEngineStatus, QueryDependentEngineGroup, ResetEngine, CancelCommand

**Checkpoint**: commit.

## Phase 3: Removal, hand-off and the rest (US2, US3)

- [ ] T003 Tests then NotifySurpriseRemoval and StopDeviceAndReleasePostDisplayOwnership
- [ ] T004 Tests then GetChildContainerId (`Edid`), SystemDisplayEnable/Write, SetPowerComponentFState, PowerRuntimeControlRequest, SetVidPnSourceAddressWithMultiPlaneOverlay; entry points; twelve slots

**Checkpoint**: commit.

## Phase 4: Scenario

- [ ] T005 Scenario through the table; docs; gate; merge; push
