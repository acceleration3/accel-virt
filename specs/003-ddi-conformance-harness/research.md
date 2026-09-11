# Research: DDI conformance harness

## R1. How Linux tests drive a Windows DDI table

**Decision**: core `DdiTable` mirrors `DRIVER_INITIALIZATION_DATA` with
fixed-width types; `src/km/ddi_table_layout.cpp` (generated) asserts size and
every slot offset against the WDK header.

**Rejected**: Wine (no ASan/UBSan; `KeQueryInterruptTime` reads kernel shared
data and faults in user mode; every kernel import needs a fake) and compiling
WDK headers for Linux (`long` is 64-bit, so every `ULONG` layout and the
headers' own `C_ASSERT`s break).

## R2. The table has 193 fields

`clang -fdump-record-layouts-simple` on the WDK struct lists 193 fields; the
reference table listed 188 because `ddi_extract.py` skipped `VOID*`/`void*`
`Reserved`, `Reserved1`-`Reserved4`. `DpiGetDriverDataSizeFromVersion` copies
0x608 = 8 + 192 x 8 bytes for WDDM 3.x, which corroborates it independently.

## R3. Registration, bring-up, teardown

Recorded with citations in [`docs/ddi-sequences.md`](../../docs/ddi-sequences.md)
(37 `Dpi*` citations cross-checked against the Ghidra function list, 0
mismatches). Registration rules implemented in `FakeDxgkrnl`:

| Rule | Source |
|---|---|
| `Version >= 0x104E` | `DpiInitializeInternal @ 140082868` |
| 13 required slots (QueryChildStatus or QueryConnectionChange) | `DpiInitializeInternal @ 140082868` |
| one registration per driver object | `DpiInitializeEx @ 1402526d8` |

Two findings for feature 006: dxgkrnl connects the interrupt before
`DxgkDdiStartDevice` and disconnects it after `DxgkDdiStopDevice`; and it calls
`DxgkDdiExchangePreStartInfo` (slot 126) at AddDevice time.
