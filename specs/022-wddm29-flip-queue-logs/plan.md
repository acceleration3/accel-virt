# Implementation Plan: WDDM 2.9 flip queue log buffers

**Branch**: `022-wddm29-flip-queue-logs` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 2.9. The flip queue log slots serve only a hardware
flip queue the driver does not request, so they stay NULL and the scenario
proves start and flips at 2.9 without them.

## Phases

1. Tests then WDDM 2.9; scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no new class |
| IV | Pass - start and flips through the table |
| VI | Pass - 2.9 requires nothing unimplemented |
| VII | Pass - no new code path |
