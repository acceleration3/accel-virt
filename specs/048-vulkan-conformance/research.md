# Research: conformance and the application suite in the guest (048)

## 1. What a `.qpa` log holds, and why it is the right input

dEQP - which the Vulkan CTS is built from - writes one log per run in its own
"qpa" format: a plain-text stream of records, each opened and closed by a
`#`-prefixed line, with XML inside. What matters here is three of them:

```
#beginTestCaseResult dEQP-VK.api.smoke.triangle
<TestCaseResult ...>
  <Result StatusCode="Pass">Pass</Result>
</TestCaseResult>
#endTestCaseResult
```

and, for a case that never finished:

```
#beginTestCaseResult dEQP-VK.api.smoke.triangle
#terminateTestCaseResult Crash
```

The third is `#beginSession` / `#endSession` around the whole run. A process
killed by a TDR or a bugcheck leaves the file open at the last
`#beginTestCaseResult`, which is precisely the case to resume *after*. So the
parser answers three things a comparison needs - which cases ran, what each
answered, and which case the run died in - from the same file, with no
bookkeeping of its own.

Status codes worth naming: `Pass`, `Fail`, `QualityWarning`,
`CompatibilityWarning`, `NotSupported`, `ResourceError`, `InternalError`,
`Crash`, `Timeout`, `Waiver`. `NotSupported` is the one the spec's first edge
case turns on: a host GPU without an extension answers `NotSupported` on both
sides, which is a match rather than a failure.

**Decision**: parse the `.qpa` directly, and treat an unterminated last case
as a case that crashed. **Alternatives**: the CSV `--deqp-log-filename` summary
(not written when the process dies), or the JUnit export (only written at the
end of a successful run) - both lose exactly the case that matters.

## 2. The verdict is a comparison, not a count (FR-002)

An absolute pass count says nothing: the same subset on the same GPU fails
differently between driver versions, and a host GPU missing an extension fails
cases that were never this driver's to pass. So the verdict is:

- **guest-only failure**: passed on the host, did not pass in the guest. This
  fails the run, and every one is named.
- **matching failure**: failed on both. Recorded in the run's output, not a
  failure.
- **guest-only pass**: passed in the guest, not on the host. Reported - it
  usually means the two runs were not the same subset.
- **missing**: present on one side and not the other. Reported as a case the
  comparison could not judge, which fails the run: a subset that ran on one
  side only is not a comparison.

## 3. Why the ratchet is separate from the comparison

The comparison needs a host run. The ratchet does not: it is what the guest
passed last time, committed, so a change that loses a case fails even when
nobody ran the host side that day. It is the same shape as
`tests/untestable_baseline.txt` - a number that may only go one way, and a
gain has to be written down in the change that earned it.

## 4. Sharding, and the runner's step timeout

A full mustpass run is hours. The runner's steps are bounded (`--step-timeout`),
and a step that runs past it fails the run rather than reporting. So the subset
is split into shards small enough that one shard is one step, and each shard's
`.qpa` is pulled before the next starts - which also means a guest that
bugchecks loses one shard's tail rather than the run.

## 5. Frames through the scanout, not through the guest (FR-004)

A guest-side screenshot goes through the guest's own composition, which is the
thing under test. The runner already takes QMP `screendump`s of the device's
scanout (`Screendump`, feature 038), and that is what a reference frame is
compared against: what the host would show on a monitor.

Comparison is per-pixel with a tolerance, because a GPU is allowed to round.
A mismatch keeps three files - the capture, the reference and a difference
image - which is what makes a failure readable a week later.

## 6. What this feature does not do here

No CTS is fetched, built or run on this machine: there is no network, and
pinning a hash from memory is pinning a guess (see plan.md). The parts that
decide a verdict are what is built and proven; the parts that need the archive
refuse and name the fetch.
