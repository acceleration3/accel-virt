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
in the kernel, `QueryInterruptTime` in Windows user mode, `CLOCK_MONOTONIC` on the
host. A guest table and the guest kernel's are therefore on one clock, and a
reader judges a frame's age against the same clock read when it copied the
table.

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
