# Thread context tracking

Every function in `src/` opens with one line:

```cpp
void IcdRoot::destroy_instance(VkInstance instance, const VkAllocationCallbacks *allocator)
{
    AV_SCOPE("IcdRoot::destroy_instance");
    ...
```

`AV_SCOPE` pushes the function onto the calling thread's frame stack in the
binary's trace table and pops it when the scope ends. While a function runs, a
reader outside the process - or outside the guest - sees which threads are in
which functions, since when, and which lock each is waiting on or holds. A
crash writes the same picture to a file before the process dies; a hang needs
no cooperation from the hung process at all, because the table is memory the
reader maps.

The name is the function's qualified name, written out rather than taken from
`__func__`, so it greps to the definition. `tools/scope_marks.py` inserts the
marks and `ctest -R scope_marks` fails when a definition has none or one naming
another function.

## What each binary does

| Binary | Table lives in | Crash | State dump | Reader |
|---|---|---|---|---|
| `rvkgpu.sys` | non-paged contiguous memory, its physical address logged at DriverEntry | bugcheck callback writes the table to the debug port | `D3DKMTEscape` `RVKD` | `rvkinspect.exe kernel ...`, or QMP `pmemsave` when the guest is wedged |
| `rvkvk.dll`, `rvkgl.dll`, `rvktest.exe` | named section `Global\rvk-trace-<module>-<pid>` (`Local\` when the process may not create a global one) | unhandled-exception filter writes `C:\ProgramData\rvk\crash-<module>-<pid>.txt` | a service thread answers `rvk-dump-<module>-<pid>` into `rvk-state-<module>-<pid>` | `rvkinspect.exe` |
| QEMU with `virtio-gpu-rvk-pci` | `/dev/shm/rvk-trace-device-<pid>`, left behind on a crash | signal handler writes `crash-device-<pid>.txt` beside it | a service thread on `/dev/shm/rvk-state-device-<pid>.sock` | the MCP server, directly |
| `librvk_executor.so` | `/dev/shm/rvk-trace-executor-<pid>` | the device's handler | through the device's socket | the MCP server, directly |

Where a table is not bound - unit tests, fuzz targets, a binary before its
root exists - `AV_SCOPE` does nothing.

## The table

Little-endian, fixed layout, version 1. A reader validates every field against
the bytes it holds before using it (`TraceSnapshot`, fuzzed).

Header, 96 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | magic `RVKTRACE` |
| 8 | 4 | version (1) |
| 12 | 4 | header bytes (96) |
| 16 | 4 | slot count |
| 20 | 4 | slot bytes |
| 24 | 4 | frames per slot |
| 28 | 4 | frame bytes (24) |
| 32 | 16 | module name, NUL-padded (`rvkgpu`, `rvkvk`, `rvkgl`, `rvktest`, `device`, `executor`) |
| 48 | 4 | process id (0 in the kernel) |
| 52 | 4 | crash thread number, 0 until a crash |
| 56 | 8 | clock at creation, 100 ns |
| 64 | 8 | clock ticks per second (10 000 000) |
| 72 | 4 | crash code (exception code, bugcheck code or signal), 0 until a crash |
| 76 | 4 | reserved, 0 |
| 80 | 8 | crash address, 0 until a crash |
| 88 | 8 | clock at the crash, 0 until one |

Slot, `56 + frames * 24` bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | owner: 0 free; the thread key while the thread is outside every tracked function; the key with bit 63 set while inside one |
| 8 | 4 | depth: frames pushed; may exceed the frames stored |
| 12 | 4 | thread number (the OS thread id; the kernel's is its `CLIENT_ID` thread) |
| 16 | 8 | entries: functions entered on this slot, ever - progress |
| 24 | 4 | last left: the name id of the outermost function most recently left |
| 28 | 4 | reserved, 0 |
| 32 | 8 | clock when that function was left |
| 40 | 8 | waiting on: the address of the lock the thread is blocked on, 0 when none |
| 48 | 8 | holding: the address of the lock the thread acquired most recently and still holds, 0 when none |
| 56 | ... | frames, outermost first |

Frame, 24 bytes:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | name id: FNV-1a 32 of the name's bytes |
| 4 | 2 | level: the IRQL the function entered at (0 in user mode) |
| 6 | 2 | reserved, 0 |
| 8 | 8 | clock at entry, 100 ns |
| 16 | 8 | the name's address in the writer's image, for a writer formatting its own table |

A reader maps a name id back to the name by hashing every `AV_SCOPE("...")`
literal in the source tree; two names with one id are a build error in
`scope_marks`.

## Concurrency

A thread finds its slot by hashing its key and probing. It claims a free or
idle slot with one compare-and-swap of the owner word to `key | active` and
gives it back on leaving its outermost function by swapping it to `key`, so the
slot keeps its last function until another thread needs it. An idle slot of
another thread is taken only when no free slot is left. Only the owning thread
writes a slot's frames; an interrupt on that thread nests above it and returns
the depth before the thread runs again, so a push raises the depth before it
writes the frame. A reader may see a frame not yet written; it is shown as
unwritten, not trusted.

## State dumps

A dump is text, one entry per line, `path key=value ...`, where `path` names the
object (`connection/0`, `connection/0/handle/12`, `adapter/0/rvk_context/3`). A
system class that owns a collection answers `report(StateWriter&)`; the root of
each binary is its `StateReporter`. A dump never waits for a lock: a lock it
cannot take at once is reported as `busy` with the holder the trace table
names.

## The clock

Every writer stamps the machine's interrupt time in 100 ns: `KeQueryInterruptTime`
in the kernel, the same value read from `KUSER_SHARED_DATA.InterruptTime` in
Windows user mode (`InterruptClock`; `QueryInterruptTime` is missing from some
guests' kernel32), `CLOCK_MONOTONIC` on the host. A guest table and the guest kernel's are therefore on one clock, and a
reader judges a frame's age against the same clock read when it copied the
table.

## As built: Windows user mode

- `ProcessTrace` (`src/um/trace/`) is started by `rvkvk.dll`'s `IcdLibrary` once
  its root exists, and by `rvktest.exe`'s `GuestProgram` at `run()`. It makes
  the section (128 slots, `TraceSection`), lays the table out and binds it, pins
  its module (`GET_MODULE_HANDLE_EX_FLAG_PIN`) so the Vulkan loader's
  `FreeLibrary` cannot unload code a scope or the service thread is in,
  installs the unhandled-exception filter and starts `StateService`. A failed
  step leaves tracing off; `rvkvk` logs it, `rvktest` runs regardless.
- Every section and event is made with `ObjectSecurity`'s DACL - full access
  for SYSTEM and Administrators, built with ntdll's `Rtl*` routines so no image
  imports advapi32 - under `Global\`, or `Local\` where the process may not
  create a global object.
- `SrwLock` marks the table: `waiting` only when `TryAcquireSRWLockExclusive`
  fails, `acquired` once held, `released` before it lets go. A state dump takes
  the root's lock with `try_acquire` and writes `root/0 busy=true` if it is
  held.
- The crash filter (`WindowsCrash`) records the crash in the table, unwinds the
  faulting thread from a copy of its `CONTEXT` (`RtlLookupFunctionEntry`,
  `RtlVirtualUnwind`, 48 frames at most) and writes `CrashReport`'s lines -
  the exception with its module and offset, the table through `TraceText`,
  each native frame with its module and offset - to
  `C:\ProgramData\rvk\crash-<module>-<pid>.txt`, then returns what the filter
  it replaced returns.
- The Vulkan ICD's dump: `root/0 busy= connections= handles= batches=
  memories=`, then per connection `connection/<i> opened= faulted= adapter=
  luid= device= context=`, its handles numbered in the registry's order
  `connection/<i>/handle/<n> kind= host= parent=<n|none>`, and under a device
  `memory/0 types_learnt= chunks= placements=`, under a command buffer
  `batch/0 recorded= capacity= failed=`. `rvktest`'s: `program/0 run=` and
  `report/0 program= passed= failed= last_scenario= last_step= finished=`.
- `rvkinspect.exe` is `InspectProgram` (`src/guest/inspect/`, unit tested over
  `MockInspectHost`, `MockKmt` and `ScriptedKernelTrace`) over
  `WindowsInspectHost`. Its kernel escapes name the adapter alone (no device,
  no context), which the driver answers (below).
- `WindowsTraceHost` and `InterruptClock` open no scope of their own: every
  scope's entry calls them, and a scope there recursed without end once a
  table was bound. `tools/scope_marks.py` exempts every `*_trace_host.*` file
  and `src/um/trace/interrupt_clock.cpp`.

## As built: the driver

- `DriverImage<KernelPlatform, KernelTraceHost>::enter` starts `DriverTrace`
  (`src/core/miniport/`) right after the platform and before the miniport, so
  every DDI's scopes land in the table: 64 slots in `allocate_contiguous`
  memory, bound for `AV_SCOPE`, and the line
  `rvkgpu: trace table at physical 0x<16 hex digits> bytes 52832` logged. The
  table is registered with `KeRegisterBugCheckCallback` (component
  `rvkgpu-trace`, the table as the buffer, so a dump carries its bytes), and
  the routine - a one-line function in `src/km/driver_entry.cpp` - writes
  `TraceText`'s lines, prefixed `rvkgpu: `, straight to the debug port
  (`Platform::log_at_bugcheck`: no lock, no `DbgPrint`). Names are read only
  inside the driver's image, which `DriverEntry` takes from the driver
  object's `DriverStart` and `DriverSize`.
- A table that cannot be allocated leaves tracing off and the driver loading
  (`rvkgpu: trace table not allocated; tracing is off`); a refused
  registration is logged and the table stays bound.
- `leave()` destroys the miniport first, whose teardown still opens scopes,
  then `DriverTrace::stop`: unbind, `TraceTable::withdraw` (the scopes still
  open - `stop`'s own and its callers' - then write nothing), deregister,
  release.
- `KernelTraceHost` (`src/km/`): the `KTHREAD` address without its top bit as
  the key, `PsGetCurrentThreadId` as the number, `KeQueryInterruptTime`,
  `KeGetCurrentIrql`, `InterlockedCompareExchange64` - all legal at any IRQL.
- `SpinLock`, `Mutex` and `WaitMutex` mark the table through `LockMarks`:
  `waiting` before the platform's acquire, `acquired` after, `released` before
  the release. Each gained `try_acquire` (`KeTryToAcquireSpinLockAtDpcLevel`
  after raising to DISPATCH, `ExTryToAcquireFastMutex`, `KeWaitForSingleObject`
  with a zero timeout) and a `*TryGuard`.
- `Adapter::escape` tells the two escapes apart by their first four bytes: the
  host-visible bind still needs a context; `RVKD` goes to `TraceEscape`, which
  answers with only the adapter named. A table request copies the window from
  the bound table (total 0 when none is bound); a state request builds the
  adapter's dump for the call in 256 KiB of **non-paged** memory - it is written
  while the reporters hold spin locks - and copies the window. A malformed
  request is `STATUS_INVALID_PARAMETER` with nothing written, a dump with no
  memory `STATUS_NO_MEMORY`.
- The dump (`Adapter::report`), never waiting for a lock - a table another
  thread holds is its line with `busy=true`:
  `adapter/0 started= present= powering_down= presented_fence= display_changed=
  aperture_bytes= host_visible= host_visible_base= host_visible_bytes=
  vsync_enables= vsync_reported=`, then
  `device_table/0` (`device/<i> handle= runtime= flags=`, `open/<j> allocation=`),
  `allocation_table/0` (`allocation/<i> handle= kind= size= opens= cpu_visible=`),
  `context_table/0` (`context/<i> handle= device= node= flags= rvk=`),
  `hw_queue_table/0` (`hw_queue/<i> handle= context=`),
  `execution_node/0 pending= last_submitted= last_completed= preemption_requested=
  preemption_fence= stopped= progress_time=` (`packet/<i> fence= owner= awaited=`),
  `rvk_context_table/0 bound=` (`rvk_context/<slot> handle= doorbell= ring_order=
  ring= fence_page= reply= state= context_id= completed= live= lost=`,
  `chunk/<k> resource= allocation=`; a context being set up is counted and not
  listed), and `display_sources/0 count= vsync=` (`source/<i> format= width=
  height= scanout= primary= visible=`). Handles are the objects' addresses,
  which are what dxgkrnl holds.
- **No size cap on the escape.** `NtGdiDdDDIEscape` (dxgkrnl.sys 10.0.26100,
  @140440d60) copies driver-private data of any `PrivateDriverDataSize` into a
  buffer of that size - on its stack up to 0x200 bytes, from pool above - and
  refuses only when that allocation fails (`STATUS_NO_MEMORY`), so
  rvkinspect's 16 KiB windows need no smaller page.

## `rvkinspect.exe`

The guest's reader, pushed with the package to `C:\rvk\rvkinspect.exe`. It
writes every answer to the file `--out` names, never to standard output, and
exits 0 when the file was written.

| Command | Writes |
|---|---|
| `list --out F` | text, one line per table found: `table pid=<n> module=<name> scope=<global\|local> exe=<path>` |
| `trace <pid> <module> --out F` | a snapshot file of kind 1 holding that process's table |
| `state <pid> <module> --out F` | a snapshot file of kind 2 holding the state dump the process wrote when asked, or kind 3 holding why it did not answer |
| `kernel-trace --out F` | kind 1, the driver's table, through `D3DKMTEscape` |
| `kernel-state --out F` | kind 2, the driver's dump of every adapter, through `D3DKMTEscape` |

A snapshot file: magic `RVKSNAP1` (8 bytes), the clock when the copy was taken
(u64), the kind (u32: 1 table, 2 state text, 3 error text), the payload length
(u32), then the payload.

On the host, the device and executor tables are read straight from
`/dev/shm`, and the device's state dump from the socket
`/dev/shm/rvk-state-device-<pid>.sock`: connect, send `state\n`, read text
until the device closes the connection.

When the guest is wedged, the driver's table is read from guest memory: the
driver logs `rvkgpu: trace table at physical 0x<address> bytes <n>` at
DriverEntry, and QMP's `pmemsave` copies that range.

## The MCP server

`tools/mcp/rvk_inspect.py` (stdio, no dependencies) serves the tables and dumps
of a guest run's work directory and of this host's QEMU processes. It reaches
the guest through the run's agent relay (`<work>/agent-relay.sock`, served by
`run_guest_suite.py` for the run's whole life) or, once the runner has let go,
the agent socket itself. Register it with
`claude mcp add rvk-inspect -- python3 tools/mcp/rvk_inspect.py`.
