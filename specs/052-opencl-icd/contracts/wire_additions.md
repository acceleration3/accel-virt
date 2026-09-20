# Contract: the ten new commands, and the event signal

## The commands

Added to `tools/codegen/profiles/opencl.toml` in this order, taking ids
`0x2006A` to `0x20073`. Ids are append-only: `Profile.command_id` reuses what
`opencl_ids.toml` holds and gives a new command `max + 1`, and the regenerated
sources, the ids file and `docs/protocol.md` move in one commit.

| # | Command | Why it is new work |
|--:|---|---|
| 1 | `clEnqueueMapBuffer` | returns `void *` - the address return |
| 2 | `clEnqueueUnmapMemObject` | `mapped_ptr` is an SVM-style address |
| 3 | `clSVMAlloc` | returns `void *` |
| 4 | `clSVMFree` | address |
| 5 | `clEnqueueSVMMap` | address with an extent |
| 6 | `clEnqueueSVMUnmap` | address |
| 7 | `clEnqueueSVMMemcpy` | two addresses, each with the same extent |
| 8 | `clEnqueueSVMMemFill` | address with an extent; `pattern` is `bytes_in` with `len = "pattern_size"` |
| 9 | `clSetKernelArgSVMPointer` | an address inside an allocation, not at its base |
| 10 | `clSetEventCallback` | the callback is dropped by the lowering; the host side must synthesise one |

## Two generator additions

**An address return.** A third classification beside `Command.returns_handle`
and `returns_void`. Encoded as a `uint64_t` like a handle; `_succeeded` is
`ret != nullptr`; `_client_failure` returns `nullptr`; `_default_body` answers
null. Without it the emitter writes `return CL_OUT_OF_RESOURCES;` from a
`void *` function, casts an `int64_t` to `void *`, and reports a failed map as
success.

**`Role::SVM_POINTER`.** `uint64_t` on the client, `write_pod<uint64_t>` on the
wire, `void *` on the server, resolved through a new `ServerDispatch` virtual:

```cpp
virtual void *svm_for(uint64_t address, uint64_t bytes) const = 0;
```

which answers null unless `[address, address + bytes)` lies inside one live SVM
allocation this session made. The profile spells it
`role = "svm_pointer"`, `len = "<the size parameter>"`, so the refusal is
generated rather than hand-written. A command whose pointer has no size names
none and the bound is the address alone.

This retires `EXEC_INFO_POINTERS` (`emit.py:114-116`), which refuses
`CL_KERNEL_EXEC_INFO_SVM_PTRS` because "this wire carries no allocation of
either" - no longer true.

## Addresses on the wire are host addresses

The guest's `SvmTable` translates: the application is handed a guest mirror, and
the ICD converts mirror base + offset to host base + offset before encoding. The
device therefore performs no translation, only a bounds check, and a pointer the
application invented is refused in the guest before it is sent and again on the
device before the host's OpenCL sees it.

## `clSetEventCallback`

The guest sends the event and the callback type; no guest function pointer
crosses. The device's `OpenClBackend` registers **its own** callback with the
host's OpenCL, which on completion records the event in `ClEventSignals` and
wakes the context through `ContextSignal`. The guest's `ClEventWatcher` wakes,
asks which events completed, and runs the application's callbacks.

## The event signal, device to guest

**Executor ABI version 5.** `RVK_EXECUTOR_ABI_VERSION 5`, with
`RVK_EXECUTOR_ABI_VERSION_EVENTS 5`, appending the first device-supplied
callback to `RvkExecutorOps` - a `signal` function pointer and the device's
opaque. `ExecutorLibrary::accept` null-checks it, `names_events()` reports it,
and `ContextExecutor::accepts` consults that before a context binds. A version 4
library keeps working and reports no events; `tests/qemu/echo_executor.cpp`
keeps its version 1 and 3 tables so the gate is exercised.

**The landing pad.** On the host driver's thread the callback may do exactly
two things: record the completion under a short mutex, and signal the worker's
wake event. It touches no fence page, no handle registry, no decode arena -
each of which is worker-only and enforced (`MockHost::require_worker`, whose
unclaimed violations fail the test binary). Waking the worker from a foreign
thread is precedented: the doorbell's KVM ioeventfd already sets that event.

**The raise.** `FenceReporter::nudge()` - worker-side - bumps `progress`,
republishes the page and sets `pending_`, so the next `signal()` raises the
context's line. `completed_fence` does not move, which is exactly what
`FenceProgress::observe` permits: it fails a context on a regression or on a
`completed_fence` past what was submitted, and says nothing about `progress`
advancing on its own.

**The page does not change.** It is 32 bytes with no room for an event id, and
needs none: the wake carries no payload and the watcher asks over the wire.

## The lifetime rule

OpenCL cannot unregister a `clSetEventCallback` callback, so a host callback can
fire after `shut_down`, after `ContextExecutor::destroy`, and after the session
is gone. Every registration therefore carries an epoch, and a callback whose
epoch is stale does nothing at all. This is the most dangerous code in the
feature and it gets a test that destroys a session with a callback in flight.
