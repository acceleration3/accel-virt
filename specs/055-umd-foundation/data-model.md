# Data model: rvkumd.dll - the user-mode driver and DXGI

## Guest user mode, portable (`src/icd/d3d`)

**UmdRoot** - one per process, in `ObjectStorage` in the exports' translation
unit (`src/um/d3d/rvkumd_exports.cpp`).
- Owns: the injected `BlockSource`, `Kmt`, two `ThreadLock`s and `LogSink`; the
  `UmdConnection`; the `AdapterTable`; the `DeviceTable`.
- Rule: nothing is opened before the first `OpenAdapter10_2` or
  `OpenAdapter12`. The connection is opened by the first open that needs it and
  closed when the last adapter closes.
- Binds itself into `UmdBinding` for its lifetime, as `IcdRoot` does into
  `icd_binding`.
- `report(StateWriter&)` writes `umd/0` with the adapter and device counts,
  then each adapter and each device. Never waits: with the lock held elsewhere
  only `umd/0 busy=true` is written.

**UmdBinding** - the one pointer a table thunk reaches the root through.
- Fields: `UmdRoot *root_`.
- Rule: every thunk was handed out by a bound root, so none asks with nothing
  bound.

**DdiVersionList** - one per runtime kind, held by the root.
- Fields: the `uint64_t` version words, and how many. A word is
  `(interface << 32) | (build << 16)` as `d3d10umddi.h` composes it.
- Today: the D3D11 list is `{ D3DWDDM3_2_DDI_SUPPORTED }` =
  `{ 0x0011002D00010000 }`; the D3D12 list is the newest build on interface
  `R8`. Both are one entry, because a version this driver has not tested is a
  version it does not name (constitution VI).
- `answer(uint32_t *entries, uint64_t *words)`: the count-then-fill protocol
  `PFND3D10_2DDI_GETSUPPORTEDVERSIONS` uses - a null array answers the count, a
  short array is refused, an exact array is filled.
- `serves(uint32_t interface_version)`: whether an `Interface` the runtime put
  in `D3D10DDIARG_OPENADAPTER` or `D3D10DDIARG_CREATEDEVICE` is one of these.

**UmdAdapter** - one per successful `OpenAdapter*`.
- Fields: `RuntimeKind` (`d3d10_11`, `d3d12`), the runtime's `hRTAdapter`, the
  recorded `D3dddiAdapterCallbacksRecord *` (never copied, see below), the
  `Interface` and `Version` the runtime asked with, the `AdapterIdentity`, and
  the number of live devices made through it.
- States: `open` -> `closing` -> gone. `pfnCloseAdapter` with a live device is
  refused (`E_FAIL`): the runtime destroys devices first, and a driver that let
  the adapter go would leave those devices naming a dead parent.
- Rule: the callbacks pointer is recorded, never the individual callbacks -
  Microsoft's own instruction for the DXGI table, and the adapter table is no
  different.

**AdapterTable** - one per process, in the root.
- Owns: `ObjectPool<UmdAdapter>` and the handles handed to the runtime.
- `adapter_of(void *handle)`: the adapter whose `pDrvPrivate` this is, or null.
  A handle this driver did not hand out is never dereferenced.
- Invariant: every adapter in the pool has exactly one live handle, and every
  handle names an adapter in the pool.

**AdapterIdentity** - one per adapter open.
- Fields: the LUID (`Kmt::adapter_luid`), the PCI vendor and device ids
  (`PciIdentity` from `AdapterSearch`), the description, the dedicated and
  shared memory (`Kmt::query_video_memory`, both segment groups).
- Rule: every figure comes from the same two D3DKMT queries the Vulkan ICD
  makes on the same adapter, so SC-002's agreement holds by construction. A
  figure the kernel refuses is reported as zero and logged once; the open does
  not fail for it.

**UmdConnection** - one per process, opened by the first adapter open.
- Owns: the rvkgpu adapter handle found by `AdapterSearch`, the
  `GuestConnection` (D3DKMT device and paging queue), a `KmtTransport` of
  `SubApi::vulkan`, the send lock, a `faulted` flag.
- States: `closed` -> `opening` -> `open` -> (`faulted`) -> `closed`.
- Rule: it is *injected into* the root rather than reached for, so feature
  053's shared link is a different object passed to the same constructor
  argument, not a rewrite (research.md §8).

**UmdDevice** - one per successful `CreateDevice`.
- Fields: its `UmdAdapter`, the runtime's `hRTDevice` and `hRTCoreLayer`, the
  recorded `D3DDDI_DEVICECALLBACKS *` (the kernel callbacks) and core-layer
  callbacks pointer, its `DxgiBaseTable`, its `DeviceRemoval`, and the
  `Interface`/`Version` it was created at.
- Rule: the device's *memory* is the runtime's (see `DeviceCookie`); the device
  *object* is in the root's pool. Nothing is placement-new'd into the runtime's
  block.

**DeviceCookie** - one per device, living in the runtime's private block.
- Fields: `uint64_t magic_` (`RVKD` = `0x444B5652`), `UmdDevice *device_`.
- `pfnCalcPrivateDeviceSize` answers `sizeof(DeviceCookie)`; `pfnCreateDevice`
  writes one into `hDrvDevice.pDrvPrivate`.
- Rule: the block is memory this driver does not own and another component may
  have scribbled on. `DeviceTable::device_of` checks the magic **and** that the
  pointer is one the table handed out, before any field of the device is read.

**DeviceTable** - one per process, in the root.
- Owns: `ObjectPool<UmdDevice>` and the set of live device pointers.
- `device_of(void *drv_private)`: the device that block names, or null for a
  wrong magic, a pointer never handed out, or a device already destroyed.
- Invariant: every device in the pool is counted on exactly one adapter, and
  the sum of the adapters' device counts is the pool's size.

**DxgiBaseTable** - one per device.
- Fields: the chosen revision (`base`, `1_1`, `1_2`, `1_3`, `1_4`, `1_5`,
  `1_6_1`), the recorded `DXGI_DDI_BASE_CALLBACKS *`, and whether the runtime
  gave a table to fill at all.
- Rule: the revision follows the DDI interface version the runtime asked for,
  not a guess; an arm of `DXGI_DDI_BASE_ARGS` narrower than the version implies
  is a refusal, because filling 21 slots into a 7-slot struct writes past the
  runtime's memory.
- Every slot is non-null. Which answer each gives is
  `contracts/dxgi_seam.md`.

**DeviceRemoval** - one per device.
- Fields: `reason_` (0 while live, else `DXGI_ERROR_DEVICE_REMOVED` or
  `DXGI_ERROR_DEVICE_RESET`), `reported_`.
- `remove(reason)` is idempotent in the reason: the **first** cause is the one
  `GetDeviceRemovedReason` names, later ones are counted and dropped.
- `check()` answers the reason, and logs once - the first time only - with the
  entry point that noticed.

## The DDI mirror records (`src/icd/d3d/ddi`)

Public fields, WDK names, no member functions; offsets proven against the real
headers in `src/um/d3d/umd_record_layout.cpp` exactly as
`src/km/ddi_record_layout.cpp` proves the kernel's. `tools/header_scan.py`
gains this directory (plan.md, amendment 2).

| Record | Mirrors | Fields 055 reads |
|---|---|---|
| `D3dddiAdapterCallbacksRecord` | `D3DDDI_ADAPTERCALLBACKS` | `pfnQueryAdapterInfoCb`, `pfnGetMultisampleMethodListCb`, `pfnQueryAdapterInfoCb2` |
| `D3d10ddiargOpenAdapterRecord` | `D3D10DDIARG_OPENADAPTER` | all six |
| `D3d10_2ddiAdapterFuncsRecord` | `D3D10_2DDI_ADAPTERFUNCS` | all five (written) |
| `D3d10ddiargCalcPrivateDeviceSizeRecord` | `D3D10DDIARG_CALCPRIVATEDEVICESIZE` | `Interface`, `Version`, `Flags` |
| `D3d10ddiargCreateDeviceRecord` | `D3D10DDIARG_CREATEDEVICE` | `hRTDevice`, `Interface`, `Version`, `pKTCallbacks`, the device-funcs union arm, `hDrvDevice`, `DXGIBaseDDI`, `hRTCoreLayer`, the callbacks union arm, `Flags`, `ppfnRetrieveSubObject` |
| `D3d10_2ddiargGetCapsRecord` | `D3D10_2DDIARG_GETCAPS` | `Type`, `pInfo`, `pData`, `DataSize` |
| `D3d12ddiargOpenAdapterRecord` | `D3D12DDIARG_OPENADAPTER` | all four |
| `D3d12ddiAdapterFuncsRecord` | `D3D12DDI_ADAPTERFUNCS` | all eight (written) |
| `D3d12ddiargGetCapsRecord` | `D3D12DDIARG_GETCAPS` | `Type`, `pInfo`, `pData`, `DataSize` |
| `D3d12ddiTableRequestRecord` | `D3D12DDI_TABLE_REQUEST` | `tableType`, `numTables` |
| `DxgiDdiBaseArgsRecord` | `DXGI_DDI_BASE_ARGS` | `pDXGIBaseCallbacks`, the functions union |
| `Dxgi161DdiBaseFunctionsRecord` | `DXGI1_6_1_DDI_BASE_FUNCTIONS` | all 21 (written) |

`D3D10DDI_HADAPTER` and `D3D10DDI_HDEVICE` are single-`void*` structures
(`D3D10DDI_H(...)`, `d3d10umddi.h:245`), and the `HRT*` handles are the same
shape (`D3D10DDI_HRT(...)`, line 142). The records carry them as `void *`,
which the layout proof pins.

## The wire (`src/proto/resources`)

**UmdPrivateData** - the envelope every later UMD private-data record nests in.

| Offset | Type | Field | Rule |
|--:|---|---|---|
| 0 | u32 | magic | `RVKU` = `0x554B5652` |
| 4 | u16 | version | 1 |
| 6 | u16 | kind | 1 allocation, 2 resource, 3 context; 0 and anything above 3 refused |
| 8 | u32 | length | the whole record including this header; at least 16, at most the caller's buffer, and never wrapping when added to an offset |
| 12 | u32 | reserved | zero |

- `encode` writes the header at the front of a caller's buffer; `decode`
  judges one and hands back a `ByteRange` over the body.
- Refused: a short buffer at every length below 16, a wrong magic, a version
  other than 1, a kind of 0 or above 3, a non-zero reserved field, a `length`
  below 16, a `length` past the buffer, and a `length` that overflows when a
  body offset is added to it.
- Hostile table plus a byte-flip sweep of a valid record; libFuzzer target
  `fuzz_umd_private_data` with a committed corpus.
- The KMD's `DxgkDdiCreateAllocation` refuses anything this decoder refuses,
  with one log line naming the offending value (FR-006 for the envelope;
  research.md §10 records why the fields are 056's and 057's).

## The harness (`tests/harness/icd`)

**FakeD3dRuntime** - `d3d11.dll` and `D3D12Core.dll` as the driver sees them.
- Owns: which runtime it is playing, the adapter handle it was given, copies of
  the two adapter tables, the callbacks it offered, the private device block it
  allocated, and a journal of every call with its arguments.
- `open(RuntimeKind, uint32_t interface_version, uint32_t runtime_version)`:
  calls the export, checks every table slot is non-null, records the handle.
- `supported_versions()`, `caps(type, data, bytes)`, `calc_private_device_size()`,
  `create_device()`, `destroy_device()`, `close_adapter()`: each calls the slot
  the real runtime calls, in the real order.
- Refusals a test sets: `offer_no_callbacks()`, `offer_a_short_dxgi_table()`,
  `ask_for_version(word)`, `scribble_the_device_block()`,
  `close_the_adapter_first()`.
- Counters: `calls(Call)`, `last_hresult()`, `device_block_bytes()`.

**UmdRig** - one UMD process.
- Members, in construction order: `MockPlatform platform_` (the `BlockSource`),
  `MockKmt kmt_`, `ScriptedHost host_`, `LockTrace trace_`,
  `RecordingThreadLock lock_{'U'}`, `RecordingThreadLock send_lock_{'S'}`,
  `RecordingLog log_`, `UmdRoot root_`.
- Constructor adds the rvkgpu adapter to `MockKmt` and sets its LUID, as
  `IcdRig` does.
- Accessors mirror `IcdRig`'s so a reader moving between the two ICDs and this
  driver reads the same rig.
