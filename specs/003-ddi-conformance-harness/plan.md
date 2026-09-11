# Implementation Plan: DDI conformance harness

**Branch**: `003-ddi-conformance-harness` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

A byte-exact core mirror of `DRIVER_INITIALIZATION_DATA` (`DdiTable`), its slot
names generated from `docs/ref/driver_init_table.md`, a generated compile-time
layout proof in `src/km`, a fake `dxgkrnl` that applies registration
validation recovered from `dxgkrnl.sys`, and `docs/ddi-sequences.md` recording
bring-up/teardown order, IRQLs and abusive sequences with citations.

## Technical Context

**Language**: C++23 freestanding (core, km); Python 3 (generator); hosted C++ (harness).
**Evidence**: `re/ghidra-proj` built from `tools/re/binaries.txt`; `tools/re/q.py`.
**Testing**: L1 unit tests for `DdiTable`; harness tests for `FakeDxgkrnl`; generator staleness test; the kernel build for the layout proof.

## Constitution Check

| Principle | Status |
|---|---|
| II | Pass - layout proof is compile-time only; no branch in km |
| III | Pass - `DdiTable` owns the table; `FakeDxgkrnl` owns validation state; generated enum is data |
| IV | Pass - tests first; the table reads no external bytes (no fuzz target needed) |
| VIII | Pass - no scenario runner and no argument mirrors until 006 |

## Classes

In [spec.md](spec.md). `DdiSlot` enumerators keep the WDK field names
(`DdiSlot::DxgkDdiAddDevice`) so a slot greps straight to its header field;
this is the one naming exception, confined to generated WDK mirrors.

## Structure

```text
tools/gen_ddi_slots.py            docs/ref/driver_init_table.md -> the two generated files
src/core/miniport/ddi_slot.hpp             generated: enum class DdiSlot (193)
src/core/ddi_table.{hpp,cpp}      DdiTable
src/km/ddi_table_layout.cpp       generated: size + 187 offset static_asserts
tests/unit/core/miniport/test_ddi_table.cpp
tests/harness/fake_dxgkrnl.{hpp,cpp}, tests/harness/dxgkrnl/registration_rule.hpp
tests/unit/harness/dxgkrnl/test_fake_dxgkrnl.cpp
tests/tools/test_gen_ddi_slots.py staleness
docs/ddi-sequences.md             evidence
```
