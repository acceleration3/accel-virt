# Implementation Plan: QEMU device: rvk capset and context lifetime

**Branch**: `029-device-capset-and-contexts` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`src/proto` gains `ContextRequest` and capset responses in `GpuResponse`. The
`Host` catalog answers a `GpuIdentity` per host GPU instead of a bare UUID, so
`DeviceCapset` can describe the bound GPU at realize. `ContextCommands` answers
GET_CAPSET_INFO, GET_CAPSET, CTX_CREATE, CTX_DESTROY and SUBMIT_3D carrying a
`ContextSetup`; `DeviceContext` keeps a `ContextBinding` and pins its blobs in
the `BlobTable`. `BlobCommands::handle` is renamed `answer` (CLAUDE.md names).
The glue offers `VIRTIO_GPU_F_CONTEXT_INIT` and one capset when the core says
so. The qtest suite exercises every path against the real device.

## Decisions

- **Setup transport.** SUBMIT_3D to the context, its buffer exactly one 32-byte
  `ContextSetup`. Later rvk slow-path commands are distinguished by their magic.
- **Sub-APIs.** Vulkan only: it is the first API executed (032); GL and CL are
  M7. The capset must offer at least one.
- **Refusing UNREF of a bound blob** rather than deferring its release: the
  guest can see and test it, and nothing outlives the context silently.
- **Errors.** Unknown or zero context id: `ERR_INVALID_CONTEXT_ID` for
  DESTROY and SUBMIT_3D; CTX_CREATE of id 0 or a live id is
  `ERR_INVALID_CONTEXT_ID` too. Every malformed or disallowed field is
  `ERR_INVALID_PARAMETER`; a missing resource `ERR_INVALID_RESOURCE_ID`;
  no slot or no thread `ERR_OUT_OF_MEMORY`.

## Phases

1. Proto: `ContextRequest`, capset responses; fuzz target.
2. Host identity catalog, `DeviceCapset`, blob pins, `ContextBinding`.
3. `ContextCommands`, device and ABI (`rvk_device_capsets`).
4. Glue, qtest suite, docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - the core decides the capset and features, the glue copies them |
| III | Pass - binding and capset are objects, the commands a system class |
| IV | Pass - every refusal unit tested and against the real device; fuzz for request bytes |
| V | Pass - setup validated against the capset and the blobs before binding |
| VI | Pass - only CONTEXT_INIT is added, because contexts now exist |
