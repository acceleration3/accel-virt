# Implementation Plan: WDDM 1.3 additions

**Branch**: `016-wddm13-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Three WDDM 1.3 DDIs implemented from state the adapter already owns, one
stubbed, two skipped. The platform gains the performance counter the GPU clock
is calibrated against.

## Phases

1. `Platform::performance_counter` (contract, mock, kernel); records and km
   proofs.
2. Tests then GetNodeMetadata, ControlInterrupt2, CalibrateGpuClock,
   SetPowerPState; entry points; four slots.
3. Scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no stateless class |
| IV | Pass - every answer and refusal tested |
| VI | Pass - no capability changes |
| VII | Pass - CalibrateGpuClock reads a counter at up to DISPATCH |
