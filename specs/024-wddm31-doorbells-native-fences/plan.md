# Implementation Plan: WDDM 3.1 doorbells and native fences

**Branch**: `024-wddm31-doorbells-native-fences` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

The adapter reports WDDM 3.1. Native fences and doorbells stand on hardware
scheduling and a user-mode driver, and doorbells on the device's doorbell BAR, so
their nine slots stay NULL and their enablement contracts are recorded for the
feature that enables hardware scheduling.

## Phases

1. Tests then WDDM 3.1; scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no new class |
| IV | Pass - start through the table |
| VI | Pass - neither native fences nor user-mode submission is claimed |
| VII | Pass - no new code path |
