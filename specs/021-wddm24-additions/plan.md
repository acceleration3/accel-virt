# Implementation Plan: WDDM 2.4 additions

**Branch**: `021-wddm24-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 2.4, where dxgkrnl sends immediate flips; the scanout
already shows a stored address from the next vsync and VidSch expects no report.
ResumeHwEngine completes the engine recovery pair. Eleven slots stay NULL.

## Phases

1. Record and km proof; tests then ResumeHwEngine, WDDM 2.4, immediate flips.
2. Scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no new class |
| IV | Pass - immediate flip and recovery through the table |
| VI | Pass - 2.4 requires nothing unimplemented |
| VII | Pass - immediate flips at device IRQL store atomically |
