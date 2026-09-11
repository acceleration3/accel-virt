---
description: "Task list for feature 003: DDI conformance harness"
---

# Tasks: DDI conformance harness

## Phase 1: DDI table (US1, P1)

- [X] T001 [US1] Write `tests/tools/test_gen_ddi_slots.py`: running `tools/gen_ddi_slots.py --check` passes on the committed files and fails naming the file when `src/core/miniport/ddi_slot.hpp` or `src/km/ddi_table_layout.cpp` differs from generation; generation from a two-row fixture table produces `Version = 0` and `DxgkDdiAddDevice = 1`. Must fail now
- [X] T002 [US1] Implement `tools/gen_ddi_slots.py` (write, and `--check`), generate and commit `src/core/miniport/ddi_slot.hpp` and `src/km/ddi_table_layout.cpp`. T001 passes
- [X] T003 [US1] Write `tests/unit/core/miniport/test_ddi_table.cpp`: new table has version 0x11007 and all routine slots null; set/read one slot leaves all others null; setting `DdiSlot::Version` or `DdiSlot{193}` returns `invalid_parameter` and changes nothing; clearing with null works; `invariants_hold` true after every mutation and false when a probe corrupts version or padding. Must fail to compile
- [X] T004 [US1] Implement `src/core/ddi_table.{hpp,cpp}`; the driver build compiles `ddi_table_layout.cpp`. T003 passes; moving one generated offset by 8 fails the driver build naming the slot (verified once, reverted)

- [X] T004a Found by the layout proof on its first build: `DRIVER_INITIALIZATION_DATA` has 193 fields, not 188. `tools/re/ddi_extract.py` dropped the five `VOID*`/`void*` `Reserved` fields, so `docs/ref/driver_init_table.md` and every slot number after 69 in the specs, roadmap and KB were wrong. Fixed test-first (`tests/tools/test_ddi_extract.py` compares against the kit); `gen_ddi_slots.py` had the same blind spot for `VOID*` rows (fixed test-first); `gen_specs.py` no longer lists `Reserved` fields as DDIs; `gen_roadmap.py` had been broken since 001's `load_table(root)` change and now has a staleness test.

**Checkpoint**: commit.

## Phase 2: Evidence (US3, P2)

- [X] T005 [US3] From `re/ghidra-proj`, recover the registration validation path (`DpiInitialize*`), adapter bring-up and teardown order with IRQLs, and the abusive sequences listed in `docs/testing.md`; write `docs/ddi-sequences.md` with a citation per rule and step; record in `research.md`

## Phase 3: Fake dxgkrnl registration (US2, P1)

- [X] T006 [US2] Write `tests/unit/harness/dxgkrnl/test_fake_dxgkrnl.cpp`: one rejecting test per recovered rule (naming the rule), one accepting test, null table, second registration per T005. Must fail to compile
- [X] T007 [US2] Implement `tests/harness/fake_dxgkrnl.{hpp,cpp}` and `tests/harness/dxgkrnl/registration_rule.hpp`. T006 passes

**Checkpoint**: commit.

## Phase 4: Polish

- [X] T008 `docs/testing.md` L2 section: registration now, scenarios with 006; `tools/gate.sh`; merge to `master`
