---
description: "Task list for feature 006: WDDM 1.0 PnP, power and adapter lifetime"
---

# Tasks: WDDM 1.0 PnP, power and adapter lifetime

## Phase 1: Skeleton (US1)

- [X] T001 Constitution 2.1.0 (III exception: core entry points, one binding pointer, `ObjectStorage`); `CLAUDE.md` and `.claude/commands/dev.md` wording; `tools/check_design.py` rule: placement new only in `src/core/support/object_storage.hpp` (test-first in `tests/tools/test_check_design.py`)
- [X] T002 Tests `test_object_storage.cpp`, `test_nt_status.cpp`, `test_miniport.cpp` (table has exactly the 006 slots set; add/remove adapter slots up to 8; unbound entry points fail safely) - must fail
- [X] T003 Implement `object_storage.hpp`, `nt_status`, `miniport_binding`, `miniport`, `src/core/ddi/entry_points.{hpp,cpp}` (stubs), `FakeDxgkrnl::register_miniport`
- [X] T004 `src/core/miniport/driver_image.hpp` (`DriverImage<PlatformType>`: platform and miniport in `ObjectStorage`, bind, `Platform::register_miniport`, unwind on refusal; tested over MockPlatform) + one-line `DriverEntry` over `DriverImage<KernelPlatform>`; `KernelPlatform::register_miniport` calls `DxgkInitialize`; `KernelPlatform::log` also writes the debug port 0x402; `driver_image` test requires the `IoGetDeviceObjectPointer` import displib's `DxgkInitialize` brings. Found: `dispmprt.h` has no C linkage guard; clang-cl lacks `__outbytestring`

**Checkpoint**: commit.

## Phase 2: Records and inputs

- [X] T005 DDI records in `src/core/ddi/` and `src/km/ddi_record_layout.cpp` (every field offset and size against the WDK); `NtStatus` gains `child_descriptor_not_supported` (0xC01E0401, replacing a wrong 0xC01D0003) and the inverse mapping for callback failures
- [X] T006 Tests then `DxgkrnlInterface` (Size check, copy, typed callbacks)
- [X] T007 Hostile tests then `ResourceList` (memory, large memory, message interrupts; bounded counts) and `PciBaseAddresses` (32/64-bit memory BARs, I/O BAR, 64-bit BAR 5); `fuzz_resource_list`; base addresses added to `fuzz_virtio_capabilities`. Found: `tools/header_scan.py` counted only trailing-underscore members, so DDI records (WDK field names) read as stateless; records in `src/core/ddi/` named `*Record` now count their fields and may not declare member functions (test-first)

**Checkpoint**: commit.

## Phase 3: Adapter lifetime (US2)

- [X] T008 `Platform` register I/O; mock routes addresses to attached `RegisterWindow`s and records `unmapped_register`; `MappedRegisterWindow`; `src/km/mmio_register_window` removed
- [X] T009 `FakeDxgkrnl` callbacks (`tests/harness/fake_dxgkrnl_callbacks.cpp`, one-line `extern "C"` thunks) over a fake virtio-gpu PCI function built from `tests/fixtures/virtio_gpu_pci_config.bin`
- [X] T010 Tests then `BarMappings`, `VirtioGpuDevice`
- [X] T011 Tests then `Adapter` start/stop/remove/power/reset, with the allocation-failure sweep. Found: `VirtioPciTransport` needed a public waiting `reset` (StopDevice, D3) and a non-waiting `abandon` (ResetDevice, destruction)

**Checkpoint**: commit (Phases 3 and 4 share one: `Adapter`'s DDIs landed together over the same fixture).

## Phase 4: Interrupts and children (US3, US4)

- [X] T012 Tests then ISR, DPC, QueryChildRelations/Status, QueryDeviceDescriptor, LinkDevice

**Checkpoint**: commit.

## Phase 5: Scenarios

- [X] T013 Fake dxgkrnl scenario runner: recorded bring-up/teardown order, abusive variants
- [X] T014 `tools/gate.sh`; docs (`docs/ddi-sequences.md` LinkDevice at AddDevice); merge
