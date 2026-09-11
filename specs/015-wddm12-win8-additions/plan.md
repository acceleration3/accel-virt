# Implementation Plan: WDDM 1.2 (Windows 8) additions

**Branch**: `015-wddm12-win8-additions` | **Date**: 2026-09-11 | **Spec**: [spec.md](spec.md)

## Summary

Twelve WIN8 DDIs registered and tested, none claimed. Per-engine TDR over the
one `ExecutionNode` following `basicrender.sys`; surprise removal makes the
adapter stop touching the device; the POST hand-off stops the device and hands
over no frame buffer; the ELD comes from the synthetic EDID; system display,
runtime power and multi-plane overlay decline as dxgkrnl tolerates.

## Phases

1. Records and km proofs.
2. Tests then `ExecutionNode::abort_pending`, engine DDIs and CancelCommand.
3. Tests then surprise removal, the POST hand-off, `Edid` ELD, the declining
   DDIs; entry points; twelve slots.
4. Scenario; docs; gate; merge; push.

## Constitution Check

| Principle | Status |
|---|---|
| III | Pass - behaviour lives on the objects that own the state |
| IV | Pass - every DDI and every refusal tested; removal sequence through the table |
| VI | Pass - no capability claimed |
| VII | Pass - FState and runtime control at DISPATCH take no lock |
