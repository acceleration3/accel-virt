# Implementation Plan: WDDM 1.1 (Windows 7) additions

**Branch**: `014-wddm11-win7-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

One DDI implemented, seven skipped. `DxgkDdiQueryVidPnHWCapability` answers
that the driver transforms nothing; the WDDM 2.0 placeholders and RenderKm stay
NULL, which the slot-exactness test already proves.

## Phases

1. Record and km proof; tests then `Adapter::query_vidpn_hw_capability`, entry
   point, slot.
2. Scenario through the table; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - no stateless class added; the adapter owns the answer |
| IV | Pass - hostile ids and handles tested |
| VI | Pass - no capability changes |
