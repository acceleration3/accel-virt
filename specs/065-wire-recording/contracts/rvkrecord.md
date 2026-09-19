# `rvkrecord` - the inspect and replay tool

```
rvkrecord print <recording> [filters] [--bytes N]
rvkrecord replay <recording> --executor <library> [--gpu <32 hex>] [filters]
```

Exit status: 0 when the whole recording was read and, for `replay`, every
answer matched; 1 for damage, a mismatch or a refused replay; 2 for a usage
error. Everything it says goes to standard output; a usage error to standard
error.

## Filters (both subcommands)

| Flag | Keeps |
|---|---|
| `--context N` | records of device context N |
| `--api vulkan\|opengl\|opencl\|transport` | the batches whose commands are that API's, and the records of contexts set up for that sub-API |
| `--fence A[:B]` | records whose fence is A, or within A..B |
| `--command TEXT` | batches holding a command whose generated name contains TEXT |

## `print`

One line per record, `<offset> <+microseconds> <context> <kind> ...`, then one
indented line per command of a batch:

```
00000040 +0.000 ctx=3 setup api=vulkan ring_order=16 doorbell=1 reply=7 version=1.1
00000090 +1.204 ctx=3 fence=1 batch 148 bytes, 2 commands
                   0x00010006 vkCreateInstance 96 bytes reply
                   0x00010007 vkCreateDevice 36 bytes
00000150 +1.980 ctx=3 fence=1 reply 8 bytes
00000180 +1.981 ctx=3 fence=1 fence completed
000001a0 +2.400 ctx=3 blob imported id=1 offset=0 size=65536 memory_type=0
```

- A command's name comes from the generated table for its API
  (`cmd_id_to_string`); an id no table knows prints as `unknown`.
- A transport command (API 0) prints its arguments decoded: `nop`,
  `import_blob resource=10 offset=0 size=65536 memory_type=0`,
  `release_blob memory=...`.
- `--bytes N` appends up to N bytes of each command's payload as hex; 0, the
  default, appends none.
- A batch whose commands stop framing prints what framed and then
  `the bytes from N are framed wrong`; that is the guest's framing, not the
  recording's, so it is not damage.
- Damage prints as the last line and sets exit status 1:
  `damaged at 000002C0: a record length passes the end of the recording`. Every
  whole record before it has already been printed.

## `replay`

Replays the recording into the library `--executor` names, on the GPU `--gpu`
names (the recording's own, when it is not given).

- A `context_setup` record creates an executor of that context's sub-API.
- A `batch` record runs each of its commands through `execute` in order.
- A `reply` record is compared with the answer the replayed batch produced;
  a difference prints as
  `ctx=3 fence=12 command 2 vkCreateDevice: replied 8 bytes, recorded 16`.
- A transport command is not replayed: the device answers those itself,
  whichever library is loaded.
- A blob imported into a context, or an `IMPORT_BLOB` in one of its batches, is
  guest memory the replay cannot recreate: that context prints `unreplayable`
  and runs no further, because a wrong answer reported quietly is worse than no
  answer. The `blob_bytes` snapshot is in the recording for a replay that can
  place it; this one does not.
- The last line is `replay: N contexts, M commands, K differences`.

## The device's properties

| Property | Meaning |
|---|---|
| `record=<path>` | write a recording there; absent or empty, recording is off |
| `record-limit=<bytes>` | the ceiling; 0 (the default) is 256 MiB |
| `record-kinds=<list>` | comma-separated kind names, default every kind but `blob_bytes` |
| `record-wrap=on\|off` | at the ceiling, wrap back to the header instead of stopping (default off) |

`run_guest_suite.py --record` sets `record=` to `recording.rvk` in the run's
work directory, beside `qemu.log`.

Its rvk scenario also keeps the warm no-op round trip the run measured in
`recording-cost.txt` there, under `off` or `on` by whether `--record` was
given, and the run that completes the pair says what a recording cost
(SC-002). The record outlives a run because the two sides are two boots.
