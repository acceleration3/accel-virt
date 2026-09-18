# Data model: conformance and the application suite (048)

## `QpaResults` (object)

One `.qpa` log.

| Field | Meaning | Rules |
|---|---|---|
| `path` | the log read | a file that is not one answers no cases rather than raising |
| `cases` | case name → status | a name is `[A-Za-z0-9_.-]+`, at most 512 bytes; a duplicate name keeps the last |
| `unfinished` | the case a run died in, or None | the last `#beginTestCaseResult` with no end |
| `session_ended` | whether `#endSession` was reached | a run that ended cleanly |

Statuses are the dEQP ones, kept as written: `Pass`, `Fail`, `QualityWarning`,
`CompatibilityWarning`, `NotSupported`, `ResourceError`, `InternalError`,
`Crash`, `Timeout`, `Waiver`. A status nobody defines is kept as it is and
compared as it is - it is never silently a pass.

`passed(name)` is `Pass`, `QualityWarning`, `CompatibilityWarning` or `Waiver`:
what dEQP itself counts as not-a-failure. `NotSupported` is neither a pass nor
a failure; it is a match only against another `NotSupported`.

## `SuiteComparison` (object)

A guest result set and a host one.

| Field | Meaning |
|---|---|
| `guest_only_failures` | passed on the host, not in the guest - the run's verdict |
| `matching_failures` | failed on both - recorded |
| `guest_only_passes` | passed in the guest, not on the host |
| `unjudged` | in one set and not the other, or answered `NotSupported` on one side only |

## `CtsRatchet` (object)

`tools/cts_ratchet.txt`: one case name per line, sorted, of what the guest
passed. Empty until a guest has run the CTS at all.

| Operation | Rule |
|---|---|
| `lost(results)` | a name in the record that the results do not pass - fails the run |
| `gained(results)` | a name the results pass that the record does not have |
| `rewritten(results)` | the record a gaining run must commit |

## `CtsShards` (object)

| Field | Meaning | Rules |
|---|---|---|
| `cases` | the subset, in file order | blank lines and `#` comments dropped |
| `per_shard` | how many cases one guest step runs | at least 1 |
| `shards()` | the lists, in order | every case exactly once, order kept |
| `after(name)` | the shard tail past a case that died | what a resumed run is given |

## `FrameReference` (object)

| Field | Meaning | Rules |
|---|---|---|
| `captured` | a PPM the runner's screendump wrote | the scanout, not a guest screenshot |
| `reference` | the committed PPM | same size, or the comparison is a failure naming both sizes |
| `tolerance` | the per-channel step a driver may round by | 0-255 |
| `differs` | how many pixels are outside the tolerance | 0 is a match |

## `CtsPins` and `CtsFetch` (objects)

The shape `tools/fetch_layers.py` uses: an archive URL and a SHA-256, then one
`file` line per binary taken out of it. `tools/cts_pins.txt` carries the shape
and no pins.
