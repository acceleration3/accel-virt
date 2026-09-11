# Implementation Plan: WDDM 3.2 additions

**Branch**: `025-wddm32-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 3.2, its declared DDI level. The one 3.2 start
requirement for a display adapter, UpdateMonitorLinkInfo, is implemented; the
harness learns it. The twenty-one 3.2 slots serve GPU partitioning, live
migration, native fences or nothing reachable, and stay NULL.

## Phases

1. Harness: the 3.2 display rule. Tests then WDDM 3.2; scenario; docs; gate;
   merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no new class |
| IV | Pass - the fake applies the 3.2 rule |
| VI | Pass - 3.2 requires nothing unimplemented |
| VII | Pass - no new code path |
