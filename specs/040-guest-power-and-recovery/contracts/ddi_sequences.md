# Contract: the sequences this feature must survive

Each is an L2 scenario over `FakeDxgkrnl`, driving the real DDI table, and each
cites what it mirrors.

## Suspend and resume (US1)

```
SetPowerState(adapter, D3, ...)        → the device is stopped and destroyed
  [no DDI call may touch the device between these]
SetPowerState(adapter, D0, ...)        → the device is built and started again
SetVidPnSourceVisibility(source, TRUE) → the primary scans out again
```

dxgkrnl keeps every allocation, context and VidPn across this; the driver's own
device object is the only thing that goes away. What the driver must not do is
answer a flip, a cursor update or a scan-line query while the device is down -
each answers `invalid_state` rather than touching a destroyed device.

## Timeout and engine reset (US3)

```
QueryEngineStatus(node 0, engine 0)    → responsive only while progress moves
   ... VidSchiCheckNodeTimeout defers exactly one check per "responsive"
ResetEngine(node 0, engine 0)          → the queue is dropped, the last
                                         aborted fence answered
   ... the other device's context keeps completing; nothing else is reset
```

`VidSchiCheckNodeTimeout` (dxgmms2.sys @1400c9b50) is what asks, and it asks
only for a device that allows long-running packets. `DdiResetEngine`
(dxgkrnl.sys @14006e0a4) checks that the driver returns at the IRQL it was
called at.

## Reset from timeout, the whole adapter (US3, escalation)

```
ResetFromTimeout()                     → every node stops accepting work
RestartFromTimeout()                   → they accept again; nothing was reported
                                         in between, because VidSch aborted it
```

## Disable and enable (US2)

```
StopDevice()                           → every context, allocation and mapping
                                         released; no pool allocation left
StartDevice()                          → as if the machine had just booted
```

Fifty times in one boot is SC-002; the driver must hold no state that grows
across them.
