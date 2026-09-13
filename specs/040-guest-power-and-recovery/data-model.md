# Phase 1 data model: what power and recovery own

## `EngineProgress` (object class) — FR-002

The node's progress as dxgkrnl's timeout check sees it.

| Field | Type | Meaning |
|---|---|---|
| `platform_` | `Platform &` | its clock, and its IRQL rules |
| `node_` | `ExecutionNode &` | whose progress this is |
| `asked_time_` | `uint64_t` | interrupt time of the last answer given |
| `asked_progress_` | `uint64_t` | the node's progress time at that answer |
| `answered_` | `bool` | whether it has ever answered |

`responsive()` (DISPATCH, no lock a DPC could wait on) is true when:

- the node has nothing queued - an idle engine is not a hung one; or
- its `progress_time()` has moved since the last answer - work is completing.

It is false when the node has work queued and has not moved since the last time
dxgkrnl asked. **The first answer is always true**: the check that asks has just
decided the node is late, and one deferral is what gives a slow host the round
trip it may already be in the middle of.

Every answer records the time and progress it was given, so two answers in the
same tick cannot both be "moved".

## `PowerTransition` (object class) — US1

What the adapter had when power went away, and nothing more.

| Field | Type | Meaning |
|---|---|---|
| `started_` | `bool` | the device was running when power went down |
| `sources_` | `uint32_t` | bitmask of sources that were scanning out |
| `held_` | `bool` | whether this holds a transition at all |

`hold(bool started, const DisplaySources &sources)` (PASSIVE) records; `release()`
answers what to put back and forgets it. A second `hold` without a `release` is
`invalid_state`: two power-downs without a power-up between them is not a
transition, it is a bug.

## `ContextLoss` (system class) — FR-003

Which contexts nobody can finish, and whether their owners know.

| Field | Type | Meaning |
|---|---|---|
| `platform_` | `Platform &` | logging, and the IRQL rules |
| `lost_[max_lost]` | `uint32_t` | the context ids that were lost |
| `count_` | `uint32_t` | how many |
| `reported_` | `uint32_t` | how many of those the owner has been told about |

`lose(uint32_t context, Status why)` (DISPATCH) records a context and logs it
once with the value that caused it (FR-007). `pending()` answers how many
losses have not been reported; `report(DxgkrnlInterface &)` (DISPATCH) tells
dxgkrnl through the notification it already uses for a device-removed context,
and counts what it told. A context lost twice is recorded once.

`max_lost` is `DeviceTable::max_devices`: a loss per device is the worst a
single transition can produce, and the table cannot hold more devices than that.

## Validation rules

- No new state survives `StopDevice`: what a transition holds is released by
  the power-up that follows it, and `StopDevice` releases it too.
- Nothing on the TDR path allocates, blocks or takes a mutex: `MockPlatform`
  enforces all three, and the tests drive it at `Irql::dispatch`.
- Every loss is logged once. A test asserts the second `lose` of the same
  context logs nothing.
