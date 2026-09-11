# Implementation Plan: WDDM 1.0 timeout detection and recovery

**Branch**: `010-wddm10-tdr-baseline` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

ResetFromTimeout stops the node (dropping its queue, refusing submissions) and
powers the virtio device down; RestartFromTimeout powers it up and resumes the
node. The node records its last progress time, which the TDR report carries.

## Design

- `ExecutionNode::reset` / `restart` / `last_submitted` / `progress_time`;
  `submit` answers `invalid_state` while stopped; `take` never yields while
  stopped. Progress: interrupt time at a submission into an empty queue and at
  every completion.
- `Adapter::reset_from_timeout` / `restart_from_timeout` reuse `power_down` /
  `power_up`; entry points and slots 32 and 33.
- `DebugReport` format 2: 32 bytes.
- Scenario over `FakeVidSch` gains a reset and restart helper.

## Phases

1. Node: tests then reset, restart, progress.
2. Adapter DDIs and report: tests then the two DDIs, report format 2, slots.
3. Scenarios, docs, gate, merge, push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - extends existing objects; no new class without state |
| IV | Pass - every branch, the reset/restart twice sequence |
| VI | Pass - no capability changes |
| VII | Pass - reset and restart PASSIVE; node methods under its spin lock |
