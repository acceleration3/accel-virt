# Contract: the private driver data the KMD distrusts

The user-mode driver hands the kernel bytes with every allocation, resource and
context it creates, through `D3DKMTCreateAllocation2`,
`D3DKMTCreateContextVirtual` and their DDI counterparts. Those bytes come from
a process that may be hostile and may be a different version of this driver, so
the KMD validates them as it validates a ring (constitution V), and
`DxgkDdiCreateAllocation` refuses anything it does not recognise with
`STATUS_INVALID_PARAMETER` and one log line naming the offending value
(FR-006).

**Read this if you are feature 056 or 057.** 055 owns the envelope below and
nothing inside it. Your allocation and resource descriptions nest in it, and
each gets its own hostile-input table and its own libFuzzer target - the
envelope's tests do not cover your fields.

## Why only the envelope

A record's fields describe a thing. The things - textures, buffers, their
formats, their tiling, their shared handles - are 056's and 057's, and writing
their format in 055 would be writing a format for objects that do not exist and
that nothing could test against. What 055 can finish, and what is worth
finishing first, is the **refusal path**: the KMD's decode, its status, its one
log line, its hostile table and its fuzz target, all proven before the first
field exists. That is a deliberate narrowing of FR-006, recorded in
research.md §10.

## `UmdPrivateData` - `src/proto/resources/umd_private_data.{hpp,cpp}`

16 bytes, little-endian, at the front of every UMD private-data blob.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | `magic` | `RVKU` = `0x554B5652` |
| 4 | u16 | `version` | 1 |
| 6 | u16 | `kind` | 1 allocation, 2 resource, 3 context |
| 8 | u32 | `length` | the whole blob, header included |
| 12 | u32 | `reserved` | zero |

`encode(ByteRange)` writes the header at the front of a caller's buffer and
answers the `ByteRange` the body goes in.

`decode(ByteRange)` judges one and answers the body's `ByteRange`. It refuses,
with `Status::invalid_parameter` and nothing written:

- a buffer shorter than 16 bytes, at **every** length from 0 to 15;
- a magic other than `RVKU`;
- a version other than 1;
- a kind of 0, or above 3;
- a non-zero `reserved`;
- a `length` below 16;
- a `length` past the buffer the caller owns;
- a `length` that overflows when a body offset is added to it.

Proven by a table test covering each row, a byte-flip sweep of a valid record,
and `tests/fuzz/fuzz_umd_private_data.cpp` with a committed corpus in
`tests/corpus/fuzz_umd_private_data/`.

## How 056 and 057 extend it

- Pick the next unused `kind`. Do not reuse 1, 2 or 3 for a different meaning,
  and do not widen the header: a header change is a `version` bump and both
  versions then decode, or the old one is refused - never silently
  reinterpreted.
- Nest your record after byte 16. Your decoder takes the `ByteRange` this one
  answers, and validates every field in it on its own: this envelope proves
  only that the blob is the right length, the right version and the right kind.
- Write your own hostile table and your own fuzz target with its own corpus.
  A `check_symbol_tests.py` failure is the reminder.
- The KMD logs the refusal **once**, at the point of decision, with the value
  that caused it (constitution IX). One line per refusal, not one per field
  checked.
