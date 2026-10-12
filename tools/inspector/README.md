# avinspector

A live debugger for an accel-virt stack, run on the host. It reads what every binary already
publishes for this (docs/trace.md): the trace tables in `/dev/shm/av-trace-<module>-<pid>` and the
state dumps on `/dev/shm/av-state-<module>-<pid>.sock`.

```
cmake -S tools/inspector -B build-inspector -G Ninja
ninja -C build-inspector avinspector avinspector-tests
./build-inspector/avinspector-tests
./build-inspector/avinspector [--source src] [--shm /dev/shm] [--interval 250]
```

It is an accel-lib application: its UI and Vulkan renderer come from the accel-lib checkout named
by `-DACCEL_LIB_DIR=` (default `~/Coding/accel-lib`), which it builds as part of its own tree. Its
code follows accel-lib's conventions, not the freestanding rules of `src/`.

## Panels

| Panel | Shows |
|---|---|
| Processes | every device and executor process, alive or not (a crashed QEMU's table is still read), with its threads and how its last dump went |
| Threads | each thread of the chosen process: running, waiting on a lock (and which thread holds it), stalled (no function entered for the stall time) or idle; the function it is in and for how long; calls per second; its whole stack |
| State | the chosen process's dump as a tree; a filter over paths, keys and values; values changed since the last round marked; any number pinned to Plots with a click |
| Connections | every av context the device holds - API, fence, batches and ring bytes per second, worker, faults - with the chosen one plotted over time; the executor's sessions |
| Surfaces | each GPU's share table: origin, kind, size (and the frame it looks like), whether the guest still holds it, published or exported, opens and writes per second; exportable images |
| Plots | the pinned series, as values or per second |

Sampling runs on its own thread (Sampling menu: interval, pause, sample now), so a binary that is
slow to dump - or wedged - never stalls the window.

The device and executor record their threads only when QEMU runs with `AV_TRACE_SCOPES=1` (or
`AV_PERFETTO=1`); without it their tables stay empty and the Threads panel says so.

## MCP

The same views, as MCP tools for a debugging agent (`source/data/McpTools.h`): `stalls` (every
stalled or lock-blocked thread with its stack and the lock's holder - start here on a hang),
`processes`, `threads`, `state` (filterable; `changed: true` for only what moved since the last
round, with old values), `connections`, `surfaces`, `series` (the history behind any number) and
`sample_now`. Verdicts and rates are the window's own: both read one `Model`.

```
avinspector --mcp                 # no window; MCP on stdin/stdout
avinspector --mcp-port 4343       # the window, and MCP on 127.0.0.1:4343 (newline-delimited JSON-RPC)
claude mcp add avinspector -- "$PWD/build-inspector/avinspector" --mcp
```
