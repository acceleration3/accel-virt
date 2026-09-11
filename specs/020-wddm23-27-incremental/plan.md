# Implementation Plan: WDDM 2.3 - 2.7 incremental additions

**Branch**: `020-wddm23-27-incremental` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 2.3. None of the twelve 2.3/2.5/2.6/2.7 slots is
reachable or required for this capability set, so all stay NULL and the
scenario proves it. Reporting 2.4 and above waits for immediate flips (021).

## Phases

1. Tests then WDDM 2.3; scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no new class |
| IV | Pass - start, display and NULL slots through the table |
| VI | Pass - 2.3 requires nothing unimplemented |
| VII | Pass - no new code path |
