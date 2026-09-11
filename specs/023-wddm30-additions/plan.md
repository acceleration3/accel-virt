# Implementation Plan: WDDM 3.0 additions

**Branch**: `023-wddm30-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 3.0. Backing stores, CPU events and flip cancellation
serve features this driver does not report, so the four slots stay NULL; the
scenario starts at 3.0 and allocates a primary.

## Phases

1. Tests then WDDM 3.0; scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no new class |
| IV | Pass - start and allocation through the table |
| VI | Pass - 3.0 requires nothing unimplemented |
| VII | Pass - no new code path |
