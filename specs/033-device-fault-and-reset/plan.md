# Implementation Plan: QEMU device: fault, reset and recovery

**Branch**: `033-device-fault-and-reset` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

`FaultCode` names why a context stopped. `RingConsumer::stop` becomes a fault:
it destroys the context's executor on the worker, has `FenceReporter` write
the faulted page, and signals the interrupt from the end of every service and
from a failed attach. `BatchExecutor` records which execution failure it hit
and moves progress through the reporter for every command executed. The qtest
suites read faulted pages, pending vectors and recovered contexts.

## Decisions

- **Progress** counts executed commands and completed batches, both
  monotonic; a fault freezes it.
- **Where the interrupt is raised**: `service()` always signals after serving;
  `attach()` signals after a creation fault. One interrupt per wake at most.
- **Codes** live in `src/proto/submission/fault_code.hpp` as an enum, so the
  guest (034) and device read the same values.
- **No new control request**: recovery is `CTX_DESTROY`, create, setup; reset.

## Phases

1. `FaultCode`; `FenceReporter::fault`, `heartbeat`; tests.
2. `BatchExecutor` heartbeat and fault; `RingConsumer` faults; harness and
   fuzz targets; context and device tests.
3. qtest: fault, isolation, recovery and heartbeat suite; fence suite
   expectations; the executor ABI's "must return"; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - fault decisions in core |
| III | Pass - reporter and executor own their new state |
| IV | Pass - every code unit tested, reachable ones on the real device |
| V | Pass - a fault stops consuming before anything else is read |
| VI | Pass - nothing new offered |
