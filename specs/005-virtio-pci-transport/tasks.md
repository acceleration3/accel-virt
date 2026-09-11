---
description: "Task list for feature 005: virtio-pci transport"
---

# Tasks: virtio-pci transport

## Phase 1: DMA memory (foundational)

- [X] T001 Tests in `tests/unit/test_mock_platform.cpp` and `tests/unit/test_dma_buffer.cpp`: contiguous allocation returns distinct non-zero page-aligned fake physical addresses; the mock translates any address inside a block (and rejects outside); allocation at DISPATCH is a violation; release of an unknown block aborts; leak detection counts contiguous blocks; `stall` at DISPATCH is legal and advances the mock clock; `DmaBuffer` two-phase initialize, `no_memory` on failure, releases in its destructor, zero-filled. Must fail
- [X] T002 Implement `Platform` additions, `MockPlatform` support, `src/core/dma_buffer.{hpp,cpp}`, `KernelPlatform` translations (`MmAllocateContiguousMemorySpecifyCache`, `MmGetPhysicalAddress`, `MmFreeContiguousMemorySpecifyCache`, `KeStallExecutionProcessor`). The physical address is a separate `Platform::physical_address(block)` call, made by core only after a successful allocation, so the shim never calls `MmGetPhysicalAddress` on null and needs no branch

**Checkpoint**: commit.

## Phase 2: Capability walk (US1)

- [X] T003 Commit fixtures and `tools/host/capture_pci_config.py`; tests in `tests/unit/test_virtio_capability_walk.cpp`: both QEMU fixtures parse to research.md R2; first-instance wins; reserved cfg_type and bar ignored; loops, pointer < 0x40, capability past 0xFF, short cap_len (16/20/24 by type), capability status bit clear -> results per spec; missing required -> not_supported; bad multiplier -> malformed; hostile sweep over the fixture. Must fail
- [X] T004 Implement `virtio_region`, `virtio_pci_layout`, `virtio_capability_walk`; `tests/fuzz/fuzz_virtio_capabilities.cpp` with both fixtures as seeds

**Checkpoint**: commit.

## Phase 3: Transport (US2)

- [ ] T005 `tests/harness/fake_virtio_device.{hpp,cpp}`: `RegisterWindow` implementations for common, notify, ISR and device regions with virtio 1.2 semantics and knobs (refuse FEATURES_OK, slow reset, needs-reset, refuse MSI-X vector, queue sizes, bad queue size)
- [ ] T006 Tests `tests/unit/test_virtio_pci_transport.cpp` for every US2 scenario, status-sequence recording, IRQL ceilings. Must fail
- [ ] T007 Implement `src/core/register_window.hpp`, `src/core/virtio_pci_transport.{hpp,cpp}`, `src/km/mmio_register_window.{hpp,cpp}`

**Checkpoint**: commit.

## Phase 4: Virtqueue (US3)

- [ ] T008 Fake device processes a queue: reads avail/desc through mock translation, completes chains into the used ring, hostile knobs for used id/len/idx
- [ ] T009 Tests `tests/unit/test_virt_queue.cpp` for every US3 scenario, wrap of 16-bit indices, allocation-failure sweep, no allocation at DISPATCH; `tests/fuzz/fuzz_virt_queue.cpp` feeding hostile used rings. Must fail
- [ ] T010 Implement `src/core/virtq_segment.{hpp,cpp}`, `src/core/virt_queue.{hpp,cpp}`, `VirtioPciTransport::activate_queue`

**Checkpoint**: commit.

## Phase 5: Polish

- [ ] T011 `tools/gate.sh`; docs; merge to `master`
