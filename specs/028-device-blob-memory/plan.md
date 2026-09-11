# Implementation Plan: QEMU device blob resources and host-visible memory

**Branch**: `028-device-blob-memory` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The core takes over the blob commands from the base class. `src/proto` decodes
them and writes responses; `Host` gains allocation, guest memory mapping, host
memory blocks placed in the hostmem region, control request reading and
responding, and the base class's resource list. `BlobTable` creates guest-backed
blobs over `GuestPageMapping`s and host-visible blobs over `HostMemoryBlock`s,
mapping them within `HostmemSpace`; `BlobCommands` answers the requests. The glue
routes `process_cmd` to `rvk_device_control`, falls back to the base class, sets
up the background hostmem mapping and refuses `blob=on`. The qtest suite reads
blobs back through BAR 4.

## Phases

1. Proto: `CreateBlobRequest`, `BlobRequest`, `GpuResponseWriter`; fuzz target.
2. `Host` extension and `MockHost`; `GuestPageMapping`, `HostMemoryBlock`,
   `HostmemSpace`.
3. `BlobResource`, `BlobTable`, `ControlRequest`, `BlobCommands`, the device and
   ABI entry point.
4. Glue, qtest suite, docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - decisions in core, glue translates |
| III | Pass - each memory kind an object, the table a system class |
| IV | Pass - every refusal unit tested and against the real device; fuzz for request bytes |
| V | Pass - guest entries and offsets bounded before mapping |
| VI | Pass - `blob=on` refused, features equal stock |
