# Contract: a shared object's identity (053)

## What an identity is

A **shared object** is one 64-bit id, minted once by one adapter's miniport,
that names one host resource for the whole device. The id is the only name the
guest ever uses: a Win32 handle resolves to it, an allocation's private data
carries it, and every transport command names it.

An id is:

- **minted** by `SharedObjectTable::mint`, at `DxgkDdiCreateAllocation`, for an
  allocation whose private data asked to be shareable;
- **carried** to another process only in the allocation private driver data
  dxgkrnl hands the opening process (kb 04);
- **refused** by everyone who did not mint it and does not hold it.

## The sequence US1 proves

1. A process creates an allocation whose private data is an
   `AllocationDescriptor` followed by a `SharedObjectRecord` with `object_id` 0.
   The miniport mints an id, writes it back into the record, and answers.
2. The process calls `D3DKMTShareObjects` and sends the NT handle to another
   process by any means the operating system provides.
3. That process calls `D3DKMTOpenResourceFromNtHandle`. dxgkrnl calls
   `DxgkDdiOpenAllocation` on its device with the *creator's* private data;
   the miniport decodes the record, finds the object, and counts the open.
4. Both processes' contexts send `OPEN_SHARED_OBJECT` with the id. The device
   exports one descriptor from whichever context published it and imports it
   into each, so both name one host resource.
5. Either process closing its handle drops one reference. The host resource
   goes when the last one does, whichever process that is.

## Refusals

| Situation | Answer |
|---|---|
| private data of exactly 48 bytes | not shared; unchanged behaviour |
| a record whose magic or version is wrong | `STATUS_INVALID_PARAMETER` at create or open |
| an `object_id` the adapter never minted | `STATUS_INVALID_PARAMETER`; no host object touched |
| an `object_id` from another adapter (its LUID) | `STATUS_INVALID_PARAMETER`; cross-adapter sharing is not claimed |
| a kind or size that disagrees with the object | `STATUS_INVALID_PARAMETER` |
| an id opened twice by one device | the second open is counted; the second close uncounts it |
| an id closed more times than opened | `STATUS_INVALID_PARAMETER`, the count unchanged |
| the creating process exits while others hold it | the object lives until the last holder closes |
| a context faulting while others hold it | that context's references drop; the object lives |
| an adapter reset | every object goes with every context; the guest's opens fail afterwards |

## What is not claimed

- sharing between adapters, or with another vendor's GPU;
- a handle that survives a device reset;
- `D3DKMT_SHAREDPRIMARYSURFACE`-style named sharing: a named handle is refused
  everywhere, as 047 already refuses one.

## The NT handle

`icd::SharedHandles` is what turns a handle into an id, and it never invents
one: a handle it did not mint and did not open is not in its table, and the
three ICDs refuse it with their own API's "invalid external handle" error.

The handle is the operating system's, so a 32-bit process and a 64-bit process
sharing one is the operating system's problem and not this driver's - the id
behind it is 64 bits on both sides and the record's layout is fixed-width.
