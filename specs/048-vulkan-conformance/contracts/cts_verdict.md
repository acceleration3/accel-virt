# Contract: what a CTS run answers (048)

## The run

`run_guest_suite.py --cts` needs the pinned CTS on disk and refuses before
booting anything without it, naming `tools/fetch_cts.py`. Given it:

1. The subset (`tools/cts_subset.txt`) is split into shards of `--cts-shard`
   cases each.
2. Each shard is pushed as a case list, run with the CTS's own per-case
   timeout, and its `.qpa` pulled into the work directory before the next
   shard starts.
3. A shard whose log ends inside a case - a crash, a TDR, a bugcheck - is
   resumed from the case *after* the one that died, at most
   `--cts-restarts` times per shard. The case that died is recorded as
   `Crash`, never dropped.
4. Every `.qpa` and every log the CTS wrote is copied into `diag/`.

## The verdict

`tools/compare_cts.py <guest.qpa> <host.qpa>`:

- exit 0 and `cts: N compared, 0 guest-only failures` when nothing passes on
  the host and fails in the guest;
- exit 1 naming every guest-only failure, one per line, and the count of
  matching failures;
- exit 2 when either side could not be read, or when the two sets are not the
  same cases - a comparison that was never made says so rather than passing.

`NotSupported` on both sides is a match. `NotSupported` on one side only is
unjudged, and unjudged cases fail the comparison: the two runs were not the
same subset.

## The ratchet

`tools/check_cts_ratchet.py <guest.qpa>`:

- exit 0 when every case in `tools/cts_ratchet.txt` still passes;
- exit 1 naming every case that was in the record and no longer passes;
- prints every case the run gained, and the line to add, so a gain is
  committed in the change that earned it.

An empty record passes: no guest has run the CTS yet, and a record that claims
otherwise would be a claim nobody made.
