# Implementation Plan: QEMU device 2D resources, scanout and EDID

**Branch**: `027-device-2d-and-display` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The rvk device inherits the base class's 2D protocol untouched; this feature
proves it. A qtest virtio-gpu driver in Python brings the real device up with no
guest and runs the protocol against stock `virtio-gpu-pci` and
`virtio-gpu-rvk-pci`, sending the driver's own request bytes where the driver
already sends them. The qemu gate configuration runs it.

## Phases

1. `tests/qemu/virtio_qtest.py`, `gpu_request_bytes`, `test_virtio_gpu_2d.py`;
   the `qemu_virtio_gpu` ctest; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - no device code added |
| IV | Pass - the real device, both variants, error paths included |
| VI | Pass - feature bits proven equal to stock |
