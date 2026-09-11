# Implementation Plan: virtio-pci transport

**Branch**: `005-virtio-pci-transport` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Core gains contiguous DMA memory (`Platform` + `DmaBuffer`), a hostile-safe
capability walk producing a `VirtioPciLayout`, a `RegisterWindow` interface,
`VirtioPciTransport` (3.1.1 initialization, feature negotiation, MSI-X
vectors, queue activation) and `VirtQueue` (split ring with a private shadow).
The harness gains `FakeVirtioDevice`, a register-accurate device with hostile
knobs, so every path runs on Linux. The kernel side is translation only.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - decisions in core; km adds MMIO window and Mm*/KeStall translation |
| III | Pass - classes in spec table; `RegisterWindow` has two implementations |
| IV | Pass - tests first; fixtures captured from QEMU; two fuzz targets |
| V | Pass - config space copied and validated; used ring validated against shadow |
| VII | Pass - add/notify/harvest at DISPATCH without allocation; setup at PASSIVE |
| VIII | Pass - split rings only; no event index, indirect or packed |

## Structure

```text
src/core/platform.hpp            + allocate_contiguous/release_contiguous, stall
src/core/dma_buffer.{hpp,cpp}
src/core/virtio_region.{hpp,cpp}  virtio_pci_layout.{hpp,cpp}  virtio_capability_walk.{hpp,cpp}
src/core/register_window.hpp
src/core/virtio_pci_transport.{hpp,cpp}  virtq_segment.{hpp,cpp}  virt_queue.{hpp,cpp}
src/km/kernel_platform.cpp       additions;  src/km/mmio_register_window.{hpp,cpp}
tests/harness/mock_platform      contiguous blocks with fake physical addresses, translation
tests/harness/fake_virtio_device.{hpp,cpp}
tests/fixtures/*.bin             captured config spaces;  tools/host/capture_pci_config.py
tests/unit/test_*.cpp            tests/fuzz/fuzz_virtio_capabilities.cpp, fuzz_virt_queue.cpp
```
