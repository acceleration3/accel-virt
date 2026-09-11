# Implementation Plan: QEMU device: decode and execute

**Branch**: `032-device-execution-and-codec` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`src/proto` gains `CommandHeader`, `ReplyHeader` and the setup's reply resource.
`rvk_executor.h` is the executor ABI; `ExecutorLibrary` validates a loaded table
and tells the capset what it offers; `ContextExecutor` owns one host executor.
At setup a context allocates `BatchStaging` and pins its reply blob; on the
worker, `RingConsumer` hands each complete batch to `BatchExecutor`, which
copies the payload once, walks it with `CommandStream`, runs commands through
the executor and writes the reply before head and the fence move. The glue
loads the library the `executor` property names; the qemu configuration builds
the echo executor the qtest suites use.

## Decisions

- **ABI.** `RvkExecutorOps { abi_version, sub_apis(), create(sub_api, uuid),
  destroy(executor), execute(executor, id, payload, size, reply, capacity,
  &reply_size) }`; `execute` answers `RVK_EXECUTED`, `RVK_UNKNOWN`,
  `RVK_UNDECODABLE`, `RVK_DEVICE_LOST`. A reply larger than `capacity` is
  `RVK_EXECUTED` with `reply_size` above it: the slot reports too-large.
- **Library lifetime.** Loaded by the glue at realize and closed at finalize,
  after the core is destroyed. The core never calls the table on the main loop
  except `sub_apis`.
- **Executor lifetime.** `RingConsumer::attach` creates it on the worker;
  `Worker::run` calls `RingConsumer::detach` before returning, which destroys it.
- **Staging.** Two host allocations at setup (main loop): `2^order` for the
  payload, reply blob size minus 32 (capped at 16 MiB) for the answer.
  Released when the context goes, after the worker is joined.
- **Unknown commands** increment a counter the tests read; not logged per
  command (a flood would be a denial of service on the log).
- **Order within a batch**: copy, execute all, write reply, then (as before)
  publish head, complete the fence, signal once per service.

## Phases

1. Proto: reply resource, `CommandHeader`, `ReplyHeader`; fuzz_proto seeds.
2. ABI and executors: `rvk_executor.h`, `MockExecutorOps`, `ExecutorLibrary`,
   `ContextExecutor`; capset from the library; device and module plumbing,
   `rvk_device_create` taking the table.
3. Execution: `CommandStream`, `BatchStaging`, `BatchExecutor`; `RingConsumer`
   and `Worker` detach; setup pins the reply blob and allocates staging;
   `fuzz_command_stream`.
4. Glue and qtest: `executor` property and dlopen; echo executor library;
   qtest suites name it; execution suite; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - every decision in core; the glue loads a library and hands over its table |
| III | Pass - library, executor, stream, staging and batch executor each an object with state |
| IV | Pass - every framing rule unit tested and fuzzed; real device via the echo executor |
| V | Pass - one copy of each payload, every header bounded by the copy |
| VI | Pass - sub-APIs reported are exactly what the executor offers; none without one |
| VIII | Pass - no executor interface in C++: one core class over the C table, tested through a mock table |
