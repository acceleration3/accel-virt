---
description: "Task list for feature 011: WDDM 1.0 VidPn topology and mode enumeration"
---

# Tasks: WDDM 1.0 VidPn topology and mode enumeration

## Phase 1: Records

- [X] T001 Records and km proofs; Linux layout tests; `query_vidpn_interface`

## Phase 2: Modes and EDID (US1)

- [X] T002 Tests then `DisplayModeList`, `Edid`; QueryDeviceDescriptor

**Checkpoint**: commit.

## Phase 3: Fake VidPn

- [ ] T003 `FakeVidPn` (VidPn, topology, source/target mode set interfaces; monitor source mode set) with leak checking, and its tests

## Phase 4: DDIs (US2-US4)

- [ ] T004 Tests then `VidPn`, `SourceModeSet`, `TargetModeSet`
- [ ] T005 Tests then `VidPnSupportCheck`, `CofuncModality`
- [ ] T006 Tests then `VidPnCommit`, `MonitorModes`, `DisplaySources` state; adapter DDIs, entry points, nine slots

**Checkpoint**: commit.

## Phase 5: Scenarios

- [ ] T007 Scenarios through the table; render core rule now missing Present
- [ ] T008 Docs; gate; merge; push
