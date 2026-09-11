---
description: "Task list for feature 006: WDDM 1.0 PnP, power and adapter lifetime"
---

# Tasks: WDDM 1.0 PnP, power and adapter lifetime

## Phase 1: Skeleton (US1)

- [X] T001 Constitution 2.1.0 (III exception: core entry points, one binding pointer, `ObjectStorage`); `CLAUDE.md` and `.claude/commands/dev.md` wording; `tools/check_design.py` rule: placement new only in `src/core/object_storage.hpp` (test-first in `tests/tools/test_check_design.py`)
- [X] T002 Tests `test_object_storage.cpp`, `test_nt_status.cpp`, `test_miniport.cpp` (table has exactly the 006 slots set; add/remove adapter slots up to 8; unbound entry points fail safely) - must fail
- [X] T003 Implement `object_storage.hpp`, `nt_status`, `miniport_binding`, `miniport`, `src/core/ddi/entry_points.{hpp,cpp}` (stubs), `FakeDxgkrnl::register_miniport`
- [X] T004 `src/core/driver_image.hpp` (`DriverImage<PlatformType>`: platform and miniport in `ObjectStorage`, bind, `Platform::register_miniport`, unwind on refusal; tested over MockPlatform) + one-line `DriverEntry` over `DriverImage<KernelPlatform>`; `KernelPlatform::register_miniport` calls `DxgkInitialize`; `KernelPlatform::log` also writes the debug port 0x402; `driver_image` test requires the `IoGetDeviceObjectPointer` import displib's `DxgkInitialize` brings. Found: `dispmprt.h` has no C linkage guard; clang-cl lacks `__outbytestring`

**Checkpoint**: commit.

## Phase 2: Mirrors and inputs

- [ ] T005 Mirrors + `src/km/ddi_mirror_layout.cpp` offset proofs; tests
- [ ] T006 `DxgkrnlInterface`, `ResourceList` with hostile tests and `fuzz_resource_list`

**Checkpoint**: commit.

## Phase 3: Adapter lifetime (US2)

- [ ] T007 `Platform` register I/O (+ mock address routing to fake windows), `MappedRegisterWindow`; remove `src/km/mmio_register_window`
- [ ] T008 Fake dxgkrnl callbacks over MockPlatform and the fake device
- [ ] T009 Tests then `Adapter` start/stop/remove/power/reset

**Checkpoint**: commit.

## Phase 4: Interrupts and children (US3, US4)

- [ ] T010 Tests then ISR, DPC, child DDIs, remaining stubs

**Checkpoint**: commit.

## Phase 5: Scenarios

- [ ] T011 Fake dxgkrnl scenario runner: recorded bring-up/teardown order, abusive variants
- [ ] T012 `tools/gate.sh`; docs; merge
