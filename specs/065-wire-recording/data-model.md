# Data model: wire recordings (065)

All integers little-endian, every structure fixed-size and opening with a
32-bit ASCII magic, reserved fields zero and rejected when not - the
conventions `docs/protocol.md` already states. The normative copy of these
tables goes into `docs/protocol.md` beside them (FR-002); this file is the
working note.

## File header - 64 bytes, once, at offset 0

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKR` = `0x524B5652` |
| 4 | u16 | major | 1; a reader refuses another |
| 6 | u16 | minor | 0; a reader accepts a higher one and reads what it knows |
| 8 | u16 | wire_major | the protocol version the recorded device spoke |
| 10 | u16 | wire_minor | |
| 12 | u32 | kinds | bit `kind - 1` set for every kind this recording keeps |
| 16 | u64 | started | the host's monotonic nanoseconds when the file was opened |
| 24 | u64 | limit | the ceiling in bytes, 0 for none; never smaller than 64 |
| 32 | u32 | wraps | times the recording wrapped back to this header |
| 36 | u32 | vendor_id | the bound host GPU's `VkPhysicalDeviceProperties::vendorID` |
| 40 | u32 | device_id | `::deviceID` |
| 44 | u32 | reserved | 0 |
| 48 | u8[16] | gpu_uuid | `VkPhysicalDeviceIDProperties::deviceUUID` |

## Record header - 32 bytes, in front of every record

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKD` = `0x444B5652` |
| 4 | u16 | kind | 1..7 below; a reader stops at any other |
| 6 | u16 | reserved | 0 |
| 8 | u32 | context | the device's context id, 0 for a record that names none |
| 12 | u32 | length | payload bytes that follow, at most 16 MiB |
| 16 | u64 | timestamp | nanoseconds since the file header's `started` |
| 24 | u64 | fence | the batch this record belongs to, 0 when it belongs to none |

A record occupies `32 + length` bytes; records are not padded.

## Record kinds

| Kind | Name | Payload | Written when |
|--:|---|---|---|
| 1 | `context_setup` | the encoded `ContextSetup` (32 or 48 bytes) | a context binds |
| 2 | `batch` | the batch's command bytes, exactly as staged for the executor | before the batch's first command runs |
| 3 | `reply` | the answer bytes, 0 or more | the batch's answer is written to the reply slot |
| 4 | `fence_completed` | empty | the fence page records the batch complete |
| 5 | `fault` | 8 bytes: u32 fault code (`FaultCode`), u32 reserved | a batch fails |
| 6 | `blob` | 32 bytes: u32 action (0 created, 1 imported, 2 released), u32 memory_type, u64 blob_id, u64 offset, u64 size | a blob is created, imported into a context, or released |
| 7 | `blob_bytes` | 16 bytes: u64 blob_id, u64 offset; then the snapshot | an import is recorded and the kind is asked for |

## Classes

### Capture - `src/device/recording/`

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `RecordingHeader` | object | the fields of the table above | The first bytes of a recording |
| `RecordHeader` | object | kind, context, fence, length, timestamp | The framing of one record |
| `RecordKinds` | object | the mask of kinds kept | Which record kinds a recording keeps |
| `RecordingLimit` | object | the ceiling, the bytes written, whether it wraps | What happens when the next record would pass the ceiling |
| `WireRecorder` | system | `Host&`, the open file, the header, the limit, the kinds, a mutex, what it refused | Writes what crosses the wire to the recording |

### Reading - `src/device/recording/`

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `RecordingReader` | system | the bytes and their length, the file header, the read offset, where the damage is | Walks a recording's records, stopping at the first damage |
| `RecordingFilter` | object | a context, a sub-API, a fence range, a command-name fragment | Whether a record is shown |
| `RecordedCommand` | object | the command header and its payload within a batch record | One command inside a recorded batch |
| `RecordingPrinter` | system | the reader, the filter, the line buffer, the byte budget | Prints a recording as lines a person reads |

### Replay - `src/device/recording/`

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `ReplayContext` | object | a recorded context id, its executor, its sub-API, its last answer | One recorded context, replayed on a library |
| `RecordingReplay` | system | `ExecutorLibrary&`, the replay contexts, the GPU asked for, the findings | Replays a recording into an executor library |

### The shim - `src/device/shim/`

| Class | Kind | State it owns | Responsibility |
|---|---|---|---|
| `PosixRecordingFile` | object | a file descriptor | A recording file on this host, for the glue's host ops |
| `MappedFile` | object | the mapping and its length | A file's bytes, mapped read-only for the tool |
| `PosixExecutorModule` | object | the opened library and its table | An executor library the tool loaded by path |

`main` in `src/device/shim/recording_tool.cpp` parses argv and calls them; it is
a C entry point and holds no logic, like every other in this repository.
