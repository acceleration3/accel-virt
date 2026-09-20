# Research: Sharing across APIs and processes (053)

Every answer below is read off this repository, the WDK headers, or the Khronos
registries already vendored here. Where a question could not be answered
without a Windows guest or a host GPU, that is said, with the layer that will
answer it.

## §1 — What crosses a process boundary, and what cannot

`D3DKMTShareObjects` makes an NT handle for a resource, a synchronization
object or one of a few other kinds; `D3DKMTOpenResourceFromNtHandle` turns it
back into per-process allocation handles in the opening process. What the
miniport sees of that is documented in kb 04, "Allocation lifetime traps":

> `DxgkDdiOpenAllocation` is called in *each* process that opens a shared
> resource, and the per-process driver data is separate from the
> per-allocation data. A remoting design must key host-side objects off the
> global allocation identity, not the per-open handle.

So the only thing that reliably reaches the second process is **the allocation
private driver data the creator wrote**. Everything else - the `hAllocation`
value, the device, the process - is different on the two sides.

**Decision.** The identity is a 64-bit id the miniport mints at
`DxgkDdiCreateAllocation` and writes into a `SharedObjectRecord` that follows
the `AllocationDescriptor` in the private data. `DxgkDdiOpenAllocation` in the
second process decodes that record and asks the adapter's `SharedObjectTable`
for the object.

**Alternative rejected.** Minting the id in the ICD and passing it out of band.
FR-001 forbids an out-of-band channel, and an id the kernel never saw is one
the kernel cannot refuse - which is exactly US1 scenario 4.

## §2 — Why the id must be refusable

A hostile process can call `D3DKMTOpenResourceFromNtHandle` with any handle
value, and can write anything into the private data of an allocation it creates
itself. Two refusals follow:

- an id the adapter's table never minted is refused (`Status::not_found`), and
  nothing on the host is touched;
- an id whose recorded kind, size or adapter LUID disagrees with the object the
  table holds is refused, because a guest that can make an image be opened as a
  fence is a guest that can make the host read a `VkSemaphore` as memory.

The LUID check is also the multi-GPU edge case: a handle from another adapter
carries another adapter's LUID, so the open fails and cross-adapter sharing is
never claimed.

## §3 — One host resource, two host devices

A guest context is a host context, and a guest device is a host device
(docs/testing.md, L4, "two devices sharing memory and a semaphore"). So "one
host object" cannot mean one `VkDeviceMemory`: two `VkDevice`s cannot share
one. It means **one exported descriptor**, from which each importer makes its
own object over the same storage. 047 already does this inside one context with
`vkGetMemoryFdKHR` and `VkImportMemoryFdInfoKHR`; the whole change is moving
the descriptor from `HostExportedFds` (per context) into `HostSharedObjects`
(per device).

**Decision.** `HostSharedObjects` owns one fd per object id and hands every
importer a `dup` of it. The refcount is the number of contexts holding the
object, not the number of fds: a context that imports the same id twice holds
one reference, as `ImportTable` refuses a duplicate id today.

**Consequence for US1 scenario 3 (the owner exits).** The device's table holds
the fd, not the owning context, so the owner's `DeviceContext` being destroyed
drops the owner's reference and leaves the object alive for the others.

## §4 — Nothing new reaches the guest as a descriptor

047's `HostExportedFds` exists because a guest could otherwise name an fd it
made up (constitution V). Shared objects avoid the question: the guest names an
object by its 64-bit id and never sees a descriptor at all. The fd is created
inside the device by `export_object` and consumed inside the device by
`import_object`; it never appears in a wire body.

The hostile surface this feature adds is therefore exactly three things: the
`SharedObjectRecord` in allocation private data (miniport side), and the 24-byte
`SharedObjectRequest` body of the three transport commands (device side). Each
gets a hostile table and a fuzz target.

## §5 — Where the transport commands go

`docs/protocol.md`, "Transport commands", already defines API 0 with ids 1-3
(NOP, IMPORT_BLOB, RELEASE_BLOB), answered by the device itself whichever
executor library is loaded. Shared objects belong there for the same reason
blobs do: the device, not the library, owns the identity.

**Decision.** Ids `0x4` SHARE_OBJECT, `0x5` OPEN_SHARED_OBJECT, `0x6`
RELEASE_SHARED_OBJECT, all carrying the same 24-byte body, framed exactly as
`BlobImportCommand` frames its two: a body of any other length is command
framing (fault 5), and every other refusal is a status in the answer with the
context running on.

**Alternative rejected.** A control-queue request instead. The control queue is
the main loop under the BQL (kb 11, "Threading"), and the export has to happen
on the *owning context's worker*, because a host `VkDevice` is that worker's.

## §6 — What the executor ABI is missing

The table (`src/device/abi/rvk_executor.h`) has `import_blob`, whose handle is
guest pages, and `imported_handle`. Neither can move a host object between two
executors: a shared object has no guest pages at all, and the descriptor has to
come out of one executor and go into another.

**Decision.** ABI version 6 adds three calls, and changes nothing else:

- `export_object(executor, kind, wire_handle, int *fd)` - a descriptor for an
  object this executor's guest already made;
- `import_object(executor, kind, fd, size, uint64_t *wire_handle)` - that
  descriptor made into an object of this executor's, answered as the handle the
  guest will name it by;
- `release_object(executor, wire_handle)` - that object destroyed.

A library of version 5 or below answers every shared-object command with
"unsupported", which is a status, not a fault - the same shape `IMPORT_BLOB`
already has for an old library.

**Alternative rejected.** Extending `import_blob` with a new `RVK_BLOB_*` kind.
`import_blob` is keyed by `blob_id` and its handle names guest memory; a shared
object is neither, and the overload would make every existing branch read two
ways.

## §7 — What each host API imports a descriptor as

| Guest API | Export | Import |
|---|---|---|
| Vulkan memory | `vkGetMemoryFdKHR`, opaque fd | `VkImportMemoryFdInfoKHR` in `vkAllocateMemory` |
| Vulkan semaphore | `vkGetSemaphoreFdKHR`, opaque fd | `vkImportSemaphoreFdKHR` |
| OpenGL memory | - (nothing in GL exports a descriptor) | `glImportMemoryFdEXT` on a `glCreateMemoryObjectsEXT` name |
| OpenGL semaphore | - | `glImportSemaphoreFdEXT` on a `glGenSemaphoresEXT` name |
| OpenCL memory | - (no export in `cl_khr_external_memory`) | `CL_EXTERNAL_MEMORY_HANDLE_DMA_BUF_KHR` property on `clCreateBufferWithProperties` |

Only Vulkan exports. That is not a gap in this design: the spec's matrix is
Vulkan→GL, GL→CL and CL→Vulkan, and in every real application the *allocator*
is the API that has an exportable allocation. An `export_object` a backend
cannot serve answers "unsupported", and the guest's export then fails with that
API's documented error, which is the "formats or sizes one API cannot
represent" edge case generalised.

GL and CL both import a **dma-buf**, not an opaque fd. Vulkan exports either.
So a shared object records which kind its descriptor is, and an importer that
cannot take that kind is refused - which is why `SharedObjectRecord` and
`HostSharedObject` both carry the kind and neither infers it.

## §8 — What the OpenGL generator is missing

`cl_khr_gl_sharing`'s commands are already in `tools/codegen/profiles/opencl.toml`
with committed ids (`clCreateFromGLBuffer` … `clEnqueueReleaseGLObjects`), so
US2 scenario 3 needs no generator work at all.

OpenGL's external-object commands are not. The import itself does not need one -
the device makes the memory object inside `import_object` and answers its name,
exactly as `IMPORT_BLOB` answers a `VkDeviceMemory` - but *using* it does:

```
glBufferStorageMemEXT, glTextureStorageMem2DEXT,
glDeleteMemoryObjectsEXT, glDeleteSemaphoresEXT,
glSignalSemaphoreEXT, glWaitSemaphoreEXT
```

Six commands, every parameter a scalar or a counted array of scalars, which is
shapes the emitter already lowers (`glDeleteBuffers` is the same shape as
`glDeleteMemoryObjectsEXT`). They go in `opengl.toml`'s `extra` with
append-only ids.

`glImportMemoryWin32HandleEXT` is deliberately **not** added. Its handle
parameter is an application's `HANDLE`, and putting one on the wire would hand
the device a value only the guest kernel can resolve; the ICD resolves it to an
object id first and the device never sees a handle.

## §9 — Where the NT handle lives in the ICD

047's `ExternalObjects` turns an exported handle back into the `VkDeviceMemory`
it names, which works inside one process and nowhere else. The handle must now
be a kernel object, and all three ICDs need the same translation, so it is one
class in `src/icd/common/`:

`icd::SharedHandles` maps a Win32 handle to the object id behind it, minting
one through `Kmt::share_object` on export and resolving one through
`Kmt::open_shared_object` on import. `ExternalObjects` keeps its job - which
`VkDeviceMemory` or `VkSemaphore` a handle names *in this process* - and gains
a second question it can answer: which object id, for a handle it did not
export.

**Three `Kmt` calls are added**, and `GdiKmt` translates each into one gdi32
function:

| `Kmt` | gdi32 |
|---|---|
| `share_object(device, allocation, handle)` | `D3DKMTShareObjects` |
| `open_shared_object(device, handle, allocation)` | `D3DKMTOpenResourceFromNtHandle` |
| `close_shared_object(handle)` | `CloseHandle` (kernel32) |

`MockKmt` scripts all three, including the refusals US1 scenario 4 needs.

## §10 — What this environment can and cannot prove

| Claim | Layer that proves it | Available here |
|---|---|---|
| the record and the bodies refuse hostile input | L1 + L3 | yes |
| the miniport mints, refuses and refcounts | L1 + L2 | yes |
| the device holds one host resource for two contexts | L1 + L4 | yes |
| an owner dying leaves the object alive | L1 + L4 | yes |
| the ICDs report the extensions only where the host has the fd ones | L1 | yes |
| two host devices really see the same pixels | L4 on a host GPU | only where the host imports (docs/testing.md) |
| two guest *processes* see the same pixels | L5 | no: one process per boot is what the runner drives |
| OpenGL imports a descriptor another API exported | L1 on a host GPU | yes: this host's GL lists both fd extensions |
| OpenCL imports a descriptor another API exported | L1 on a host GPU | no: this host's OpenCL is rusticl, which has no `cl_khr_external_memory_dma_buf` |
| an OpenCL context shares an OpenGL object | L1 + L5 | no: the host CL context is not made from a GL context, and nothing on the wire names one (§18) |
| a host driver refusing an import is survived | L1 on a host GPU | no: radv refuses nothing (§19); the branch carries this feature's one `AV_UNTESTABLE` |
| a Win32 handle crosses `D3DKMTShareObjects` and comes back | L5 | **yes**: `rvkvkprobe`'s twenty `shared.*` steps, both boots green |

SC-001 and SC-003 are L5 claims that need two guest *processes* and the
extensions that would carry the objects between them. The guest is no longer
skipped - the gate runs both boots against the prepared image, and the probe's
`shared.handle` and `shared.released` steps are the kernel half of US1 proven
on a real dxgkrnl - but the interop matrix and DWM composition wait on the
second process and on the rows §12, §18 and T019 leave unreported.

The OpenCL row is the one entry that changed a task rather than a report. The
device's `import_object` for OpenCL is left unbuilt, not built and untested:
every branch past "does this device list the extension" would be unreachable by
any test on this machine, and the coverage gate is the floor, not the goal. A
host whose OpenCL carries `cl_khr_external_memory_dma_buf` is what T019 waits
for; until then a guest opening a shared object on an OpenCL context is
answered status 4, which is the same answer it gets for an export.

## §11 — What an extension row promises, found while building T034-T036

Written down because two other features asked for it and a report is not the
tree. The normative table is in `contracts/wire_additions.md`, "Extension
reporting"; this is why it reads the way it does.

**A host extension is a condition, not a row.** `GL_EXT_memory_object_fd` and
`GL_EXT_semaphore_fd` are the *host's* names and are never reported to a guest:
a guest has no file descriptors, which is the whole reason the Win32 spelling
exists. What a guest is told about is six names - the four GL ones and the two
OpenCL ones - beside the two Vulkan ones 047 already reported. Reading the
right-hand column of that table as a list of extensions is the mistake this
paragraph exists to stop.

**A row promises every command in its extension, and four of them do not keep
that promise yet.** `GL_EXT_memory_object` has 19 commands in the registry and
053 answers 4; `GL_EXT_semaphore` has 9 and 053 answers 4. `opengl.toml`
already records this trap for sparse textures - "an extension may be claimed
only when every command it needs is generated, so a command left out here is an
extension the guest is told it does not have however much the card can do" -
and 053 is the second feature to meet it. The difference is that the generator
enforces its own rule for what it *generates*, while `GlExtensionPolicy` is a
hand-kept table that will happily report a name whose commands are absent.

T037 settles it: either the profile takes the remaining 15 and 5 commands, or
the policy reports something narrower than the base extensions. Until it does,
a conformance subset should carry `cl_khr_gl_sharing` and the two `_win32` rows
and leave the two base rows alone, or it will enforce a promise 053 has not yet
made good - `glTexStorageMem3DEXT`, `glIsMemoryObjectEXT` and a dozen others
resolve to null today.

## §12 — Why T037 reports no OpenGL extension yet, and what it would take

§11 left T037 a choice: take the remaining 15 and 5 commands so the base
extensions can honestly be reported, or report something narrower. The answer
turned out not to be a choice about effort. It was measured by adding all of
them to the profile and reading what fell out.

**The generator refuses five lengths, and those are easy.** `params` on the two
memory-object and two semaphore parameter commands has no length in the
registry at all, and each is one value; `glGetUnsignedBytei_vEXT.data` is a
`COMPSIZE(target)` the rules cannot read. Five lines in the profile.

**One length it does *not* refuse, and that one is a bug.**
`glGetUnsignedBytevEXT.data` is `COMPSIZE(pname)`, which the generator accepts
because it has a rule for it - `pname_count`, whose default for an unknown
pname is **1**. `GL_DEVICE_UUID_EXT` and `GL_DRIVER_UUID_EXT` are 16 bytes and
`GL_DEVICE_LUID_EXT` is 8. Generating that command without adding those three
to `[pname_counts]` would size a UUID at one byte and let the driver write past
what the decode arena handed out. Nothing would have caught it: the generator
refuses only what it cannot read, and it *can* read this one - wrongly. If
those commands are ever added, the three counts go in with them.

**And one thing that is not a length at all.**
`GL_DEDICATED_MEMORY_OBJECT_EXT` and `GL_PROTECTED_MEMORY_OBJECT_EXT` are set
with `glMemoryObjectParameterivEXT` *before* `glImportMemory*`: the object's
storage is fixed at the import, so the parameters have to be in place first.
053's device makes the memory object *inside* `OPEN_SHARED_OBJECT`, so at the
moment those parameters are set there is no host object to set them on. This is
the same shape as the Vulkan semaphore finding in T031 - `OPEN_SHARED_OBJECT`
makes a new object where the API needs "import into this existing one".

So reporting `GL_EXT_memory_object` today would promise an ordering the design
cannot keep, and `GL_EXT_memory_object_win32` cannot be reported alone because
the registry has it require the base extension. T037 therefore reports nothing
for OpenGL, and the machinery T034-T036 built waits on one of two things:

1. **The ICD answers the three memory-object and three semaphore
   queries/parameters locally** - `glIsMemoryObjectEXT` and `glIsSemaphoreEXT`
   from the table (a created name is a memory object whether or not it was
   imported), the parameters recorded on the row, and an import refused with
   `GL_INVALID_OPERATION` when the application asked for dedicated or protected
   memory that this path cannot give it. The ten storage commands substitute
   the host's name as the four in T036 do. That is honest and reportable, and
   is the smaller of the two.
2. **Or the device creates the object at `glCreateMemoryObjectsEXT`** and the
   open imports into it, which needs an executor ABI call that does not exist
   and would also settle the Vulkan semaphore case.

Until one of them lands, a conformance subset should carry `cl_khr_gl_sharing`
and nothing of OpenGL's - not even the `_win32` rows, which §11 offered before
this was measured.

**Superseded by §20: option 1 was built, and all four rows are reported.**

## §13 — The 88-byte carrier refusal: dxgkrnl refuses *after* the driver agrees

The shareable carrier (`AllocationDescriptor::encoded_size` + 40 bytes of
`SharedObjectRecord` = 88) is refused by `D3DKMTCreateAllocation2` with
`STATUS_INVALID_PARAMETER` while the same descriptor at 48 is taken. Four guest
runs narrowed it; this section records what the fourth one actually proved,
including a wrong reading of the third that cost a boot.

### The sweep, and why its 88 row was misread

`SharedObjectProbe::carrier_size_steps` creates at 48/56/64/72/80/88/96/128
with **the same bytes every time** - a valid 48-byte descriptor followed by
zeros. The measured answers were:

| bytes | to the caller | who answered |
|---|---|---|
| 48 | `0x00000000` | taken |
| 56, 64, 72, 80 | `0xC00000BB` | the driver, `not_supported` (no shape) |
| 88 | `0xC000000D` | **the driver**, `invalid_parameter` |
| 96, 128 | `0xC00000BB` | the driver, `not_supported` (no shape) |

The first reading of that table was "dxgkrnl refuses 88 and nothing else". It
does not. 88 is the one swept length the driver *has* a shape for, so it takes
the length, reaches `AllocationTable::share`, decodes a record from the zero
bytes, fails the magic check and returns `invalid_parameter`. The
`not_supported`/`invalid_parameter` split added for exactly this purpose only
separates lengths the driver does **not** know; at a length it does know, its
own content refusal wears dxgkrnl's code.

The rule this leaves behind: **`0xC00000BB` proves the driver was reached;
`0xC000000D` proves nothing about who answered.**

### The length is exonerated outright

The sweep sent **96 and 128** bytes and the *caller* received `0xC00000BB` -
a code only this driver produces, and only from `create_one`. So dxgkrnl
carries private driver data of at least 128 bytes to the driver without
refusing it, and 88 is not a length it objects to. The four runs spent hunting
a length threshold were hunting something that does not exist.

### What the same run says about who refused the carrier

Three steps reported `0xC000000D` to the caller - `carrier_size_88`,
`carrier_long` and `carrier_shared`. The whole of `rvkgpu.log` contains
**exactly one** `CreateAllocation status 0xC000000D` line. So at least two of
those three refusals were not the driver's, and by the paragraph above the
driver was called for all three.

The one that is the driver's is `carrier_size_88`: filler, so `share` decodes a
record from zeros and fails. The other two carry a valid record, so `share`
decodes, mints, writes back and `create_one` returns `ok`.

That leaves one reading: at 88 bytes **`DxgkDdiCreateAllocation` succeeds and
`D3DKMTCreateAllocation2` fails anyway**. dxgkrnl is not refusing before the
call, it is refusing after the driver agrees. This driver has met that shape
before - `Allocation::report_to` carries a comment recording three experiments
where VidMm answered `STATUS_INVALID_PARAMETER` at `CreateAllocation` for an
allocation the driver had already accepted.

It also means the length question is *not* fully closed, only relocated: the
only length at which the driver has ever succeeded is 48, so dxgkrnl's
post-success path has never been exercised at any other length. Whether it
minds the 88 or minds what `share` did to the buffer is the open question, and
`report_to` is identical in both cases, so the suspicion falls on the write-back
into `pPrivateDriverData` - the one thing the 88-byte path does that the
48-byte path does not.

### Confirmed: the driver succeeds and dxgkrnl refuses anyway

The instrumented boot settled it. `rvkgpu.log`, counted by length:

| bytes | status | count |
|---|---|---|
| 48 | `0x00000000` | 26 |
| 56, 64, 72, 80 | `0xC00000BB` | 1 each |
| **88** | **`0x00000000`** | **3** |
| 88 | `0xC000000D` | 1 |
| 96 | `0xC00000BB` | 2 |
| 128 | `0xC00000BB` | 1 |

Three 88-byte creates the driver *accepted*, against the two 88-byte steps that
failed (`carrier_long`, `carrier_shared`) plus the ICD's own carrier inside
`shared.exportable`. The single `0xC000000D` at 88 is the sweep's filler, as
predicted. So **`DxgkDdiCreateAllocation` returns success and
`D3DKMTCreateAllocation2` answers the caller `STATUS_INVALID_PARAMETER`** -
VidMm refusing an allocation this driver already accepted, the same shape
`Allocation::report_to`'s comment recorded from three earlier experiments.

`carrier_size_96_valid` answered `0xC00000BB`, the same as the filler at 96, so
the driver's refusal there is about the shape and not the bytes. The sweep has
no assumptions left in it.

### What is left, and what it is not

`report_to` is provably identical between the 48- and 88-byte paths:
`published_as_blob()` reads only `descriptor_.kind()`, `shared_as` touches
nothing it reads, and both paths carry the same descriptor. So the fields the
driver reports are not the difference.

That leaves exactly two things dxgkrnl can see, and only one of them is new:

1. `PrivateDriverDataSize` is 88 rather than 48 on a call the driver accepted.
   The sweep cannot speak to this - it only ever got a *success* out of the
   driver at 48, so dxgkrnl's post-success path has never run at any other
   length.
2. `share()` wrote 40 bytes into `pPrivateDriverData` at offset 48. This is the
   only thing the 88-byte path does that the 48-byte path does not.

Separating them needs the driver to succeed at 88 *without* writing back, which
no probe can arrange on its own: `share()` always writes when it mints. The
cheap version of the experiment is the other way round - have the driver
re-encode the descriptor over itself on an ordinary 48-byte create. If that
starts failing, writing to `pPrivateDriverData` is forbidden outright and the
length was never involved; if it does not, the suspicion narrows to writing at
or past offset 48, or to the length itself.

Worth knowing before that boot: the guest runs `rvkgpu.sys` under **Driver
Verifier** with special pool, DDI compliance checking and security checks
(`tools/guest/provision.ps1:226`, `diag/verifier.txt`). It sits between the
driver and dxgkrnl on every call, and a run with `verifier /reset` is itself a
one-variable experiment.

If the write-back is the cause, the fix is a design question rather than a
patch: the record in `pPrivateDriverData` is how the identity reaches both the
creating process and the opening one (`docs/kb/04-memory.md`), so dropping the
write needs another channel for the creator's id. `Adapter::escape` already
exists and is magic-tagged, which is the WDDM answer to exactly that.

### The instrument

The reading above turns on which log line belongs to which call, and the log
did not say. So `Adapter::log_allocation` now puts the private-data length on
the line:

```
rvkgpu: CreateAllocation bytes 88 status 0x00000000
```

A length that reached the driver names itself; one dxgkrnl refused leaves no
line at all. `bytes 88 status 0x00000000` present in a run where `carrier_long`
reports `0xC000000D` is the whole argument above, read rather than inferred.

Its absence would mean something else again, and worth naming in advance: that
`share`'s write-back into `pPrivateDriverData` does not return, so
`Adapter::create_allocation` never reaches its log line and the eleven lines
were a miscount. That would make the write-back itself the bug - the guest is
running under Driver Verifier with special pool, DDI compliance checking and
security checks on `rvkgpu.sys` (`diag/verifier.txt`), which is worth knowing
about every refusal this driver reads from dxgkrnl. The two readings differ by
one log line, which is why the line is the experiment.

`carrier_size_96_valid` sends a descriptor **and a real unassigned record** at
96, the length the sweep sent filler at. It cannot reach the post-success path,
because the driver refuses 96 for its shape either way; what it settles is that
the refusal at 96 is about the shape and not the bytes, which is the last thing
still taken on trust about the sweep.

## §14 — The two experiments in hand, and what each answer means

§13 left one candidate: `share()`'s write into `pPrivateDriverData`, the only
thing the 88-byte path does that the 48-byte path does not. Two experiments are
built and waiting on a boot. They are independent, and having both matters -
if the second convicts the verifier, the first's answer changes meaning.

### (a) The 48-byte path made to write

`AllocationTable::create_one` now writes the descriptor back over a **client**
allocation's own bytes, changing not one of them, before it reports. The probe
runs a matched pair:

| step | length | kind | driver writes |
|---|---|---|---|
| `carrier_plain_unwritten` | 48 | shadow | no |
| `carrier_plain` | 48 | client | yes |

A shadow surface and a client allocation report identically - neither is
scanned out and neither is published as a blob, so `report_to` puts both in the
same segment with the same flags and size. The write is the only difference.

- **`carrier_plain` fails, `carrier_plain_unwritten` passes** → writing to
  `pPrivateDriverData` is refused outright and the length was never the
  variable. The whole carrier design has to find another channel for the minted
  id; `Adapter::escape` is the WDDM one.
- **Both pass** → writing below offset 48 is fine, and the refusal is about
  writing at or past it, or about the 88 itself.
- **Both fail** → something else changed and the run is void.

The scope is deliberate. The display path's own allocations - the ring, the
fence page, the reply slot - are left alone, because an experiment that stops
the driver starting produces no reading at all.

This is an experiment and not a design: it comes out once it has answered, and
the unit test that pins it says so, so that what it was stays on the record.

### (b) A boot with no Driver Verifier

`provision.ps1`'s install stage has run `verifier /standard /driver rvkgpu.sys`
unconditionally since the guest existed, so every measurement this feature has
ever taken was taken through it. Driver Verifier's DDI compliance checking
interposes on every call, and "the driver returned success and the caller got a
refusal" is the shape an interposer produces. That is too plausible a cause to
leave unremovable.

`run_guest_suite.py --no-verifier` passes `-NoVerifier` to the install stage,
which skips the line and says which driver it installed. It is refused with
`--power-cycles`, whose pool-leak check reads what the verifier tracked: with
no verifier that scenario reads nothing and passes, and a scenario that cannot
fail is worse than one that is not run.

If a `--no-verifier` boot takes the 88-byte carrier, the verifier is convicted
and (a)'s answer is about the verifier's own buffer handling rather than
dxgkrnl's. If it refuses it exactly as before, the verifier is eliminated and
every earlier measurement stands.

## §15 — Answered: dxgkrnl restores the caller's bytes before the create-time open

Read out of `dxgkrnl.sys` (Ghidra, `re/ghidra-proj`), and it accounts for every
observation without a residue.

### What CreateAllocation actually does

`DXGDEVICE::CreateAllocation` captures each allocation's private driver data
**twice**. At `1403516c8`, for every element of `pAllocationInfo`:

```c
if (two_copies) size *= 2;
buffer = operator_new__(size, 'DxgK', 0x102);
driver_copy[i]   = buffer;            // what the miniport is given
pristine_copy[i] = buffer + size;     // the caller's bytes, kept aside
memcpy(pristine_copy[i], user_bytes, size);
```

It then calls `CreateDriverAllocations` - which calls
`DxgkDdiCreateAllocation` - and immediately afterwards
`DXGDEVICE::OpenAllocations` at `140371600`, in the same syscall. That second
function is the whole answer. Before it calls the miniport again it does this,
per allocation:

```c
open_info[i].hAllocation          = allocation->handle;
open_info[i].PrivateDriverDataSize = user_info[i].PrivateDriverDataSize;   // 88
open_info[i].pPrivateDriverData    = driver_copy[i];
memcpy(driver_copy[i], pristine_copy[i], size);   /* <- the caller's bytes,
                                                     over whatever the
                                                     miniport just wrote */
ADAPTER_RENDER::DdiOpenAllocation(...);
```

and only after the open returns does it copy the buffer out to user memory and
refresh the pristine copy. So:

**Whatever `DxgkDdiCreateAllocation` writes into `pPrivateDriverData` is
overwritten before `DxgkDdiOpenAllocation` sees it, and never reaches the
caller. The write that reaches the caller is the one made during the open.**

### Why that fails this driver at 88 bytes and not at 48

1. `DxgkDdiCreateAllocation` mints an id, writes the record, records it on the
   allocation, returns success - and is logged `bytes 88 status 0x00000000`.
2. dxgkrnl restores the caller's bytes: the record is **unassigned** again.
3. `DxgkDdiOpenAllocation` runs on the creating device. `DeviceTable::claim`
   sees `allocation.shared_id() != 0`, decodes the restored record, and
   compares: `record.object_id()` is 0 and the allocation's is not, so it
   answers `invalid_parameter`.
4. `Adapter::open_allocation` is **the one DDI in this driver with no log
   line**, so the refusal left no trace.
5. `D3DKMTCreateAllocation2` returns that status: `0xC000000D`.

At 48 bytes `allocation.shared_id()` is 0, `claim` returns `ok` on its first
line, and nothing of this runs. That is the whole of the difference, and it is
why every experiment that varied the length or the write found the boundary
between 48 and 88 without finding a cause.

It also explains the four boots that read as "the driver succeeded and dxgkrnl
refused": the driver did succeed, at the DDI that logs, and then failed at the
DDI that does not.

### What it means for the design

The record in the allocation's private driver data is still the channel - but
the **open** is where it must be written, not the create. The create is where
the identity is minted, because that is where the allocation exists; the open
is where the bytes survive.

So `claim` gains one case: a record that decodes and is **unassigned**, on an
allocation that already has an id, is the creating device's own open. It opens
the object by the allocation's id and writes the minted record back, which is
what dxgkrnl then copies out to the caller. A record that *is* assigned must
still name the same object - that is the cross-process case and is unchanged.

This is not a new hole. An opener has to resolve the allocation handle before
`claim` runs at all, and on the cross-process path that handle comes from the
NT handle; the record was never the capability.

## §16 — D3DKMTShareObjects shares a *resource*, and the carrier is not one

With §15's fix in, the carrier is created, the identity is minted and the
record reaches the caller. The next call refuses:

```
rvkvk: vkAllocateMemory: no NT handle for a shared object, NTSTATUS 3221225485
```

`3221225485` is `0xC000000D` out of `D3DKMTShareObjects`. Read out of
`dxgkrnl.sys`, `DxgkShareObjectsInternal` at `14034e3c0` validates, in order:

| check | refusal |
|---|---|
| a process context | logged, `0xC000000D` |
| `count - 1 < 3`, so 1, 2 or 3 objects | "Input handle count is out of range" |
| `pObjectAttributes != nullptr` | "Invalid object attributes" |
| the first handle is not 0 | "Input object handle is NULL" |
| `HMGRTABLE::GetEntryType(handle) == 4` | **nothing at all** |

That last one is ours. There is no message for it: the type check falls
straight through to `0xC000000D`, which is why the ICD log carries a status and
the driver log carries nothing. Type 4 is the resource type - the branch it
guards calls `CreateSharedResourceNtObject`, whose locals are `DXGRESOURCE` and
`DXGSHAREDRESOURCE`, and the two- and three-object forms add a keyed mutex and
a sync object (the D3D11 trio), each of which must "use NT security sharing".

**So `D3DKMTShareObjects` shares a resource, and the carrier is a bare
allocation.** `GdiKmt::create_shared_allocation` passes `hResource = 0` and
sets no flags, so no resource is created and the handle handed to
`D3DKMTShareObjects` is an allocation's. Nothing about the object attributes is
wrong - the hand-written `ShareAttributes` is the correct x64
`OBJECT_ATTRIBUTES`, 48 bytes with the right offsets, and dxgkrnl only checks
it for null before handing it to the object manager. The suspicion recorded
against it is cleared.

### What it takes

The carrier has to become a shared resource, which is a change on both sides:

- **Guest.** `create_shared_allocation` sets `Flags.CreateResource`,
  `Flags.CreateShared` and `Flags.NtSecuritySharing`, and shares the
  `hResource` that comes back rather than `hAllocation`.
  `SharedHandles::mint` then mints over the resource, and the open side becomes
  `D3DKMTOpenResourceFromNtHandle` rather than an allocation open.
- **Miniport.** `DXGKARG_CREATEALLOCATION::hResource` is in/out: with the
  resource flag set, the driver has to answer a handle of its own and destroy
  it when the resource goes. `DxgkargCreateAllocationRecord` already carries
  `hResource` and `Flags`, so the DDI surface is there; what is missing is an
  object behind it and the destroy path that releases it.

This is a data-model change rather than a patch, and it is the last one this
feature needs: the record, the identity and the wire are all working, and what
is left is the kernel object the handle is minted over.

## §17 — A resource and an allocation are handles of different types, and a destroy checks

§16 made the carrier a resource, which settles what `D3DKMTShareObjects` takes.
It also changes what every *other* call that names the carrier takes, and the
kernel is not quiet about the difference: it checks the handle's type and
refuses the mismatch.

`ValidateDestroyAllocation` at `1403f7528` in `dxgkrnl.sys` runs one of two
paths, decided by whether `hResource` is 0:

| `hResource` | what is walked | the type it must be | the refusal |
|---|---|---|---|
| 0 | every handle in `pAllocationList` | 5 (allocation) | `DxgkLogTriageEvent(..., L"Handle type mismatch")`, then `0xC000000D` |
| non-zero | `hResource` alone | 4 (resource) | the same message, then `0xC000000D` |

Type 4 is the type `DxgkShareObjectsInternal` demands (§16), so the two agree:
4 is a resource and 5 is an allocation, and neither call takes the other's
handle. The list path has two further rules worth recording, because they say
what a caller may and may not do with a resource's allocations: every handle in
one list must belong to the *same* resource, and a resource whose first
pointer-sized field has bit 32 set refuses the call with `0xC000000D` before
anything is destroyed. What that bit marks was not read out; what matters here
is that destroying a resource's allocations one at a time is a path the kernel
guards, and naming the resource is not.

**What this feature had wrong.** `create_shared_allocation` answers the
resource, and three callers then handed that number to `destroy_allocation`,
which builds an allocation list: the Vulkan ICD's unwind when a publish fails
and its `release_shared` when the application frees exported memory, and the
in-guest probe's release of the carrier it made. Each would have been refused
with `0xC000000D` in a guest, leaving the carrier and its resource for the life
of the process - a leak the host never sees, so nothing would have reported it.
`SharedHandles::adopt` had the same shape from the other end:
`D3DKMTOpenResourceFromNtHandle` answers `hResource` as well as the allocation,
and the opener must give the *resource* back, or the resource outlives every
handle that named it.

**What it takes.** `open_shared_object` answers the resource rather than the
allocation in it, `SharedHandles` destroys that, and every carrier is released
with `destroy_shared_allocation`. The rigs are where this is proven: `MockKmt`
and `DdiKmt` now remember whether an object was asked for as a resource and
refuse each call the handle's type does not fit, so a caller that names the
wrong one fails in a Linux unit test rather than in a boot.

## §18 — Answered: what `cl_khr_gl_sharing` needs, and why it is not a table

T039 asked for the acquire window an OpenCL context keeps over the GL objects
it shares. Building it turned out to be the easy half of a question the design
cannot answer yet, and the answer is worth writing down rather than shipping a
table nothing can open.

`clCreateFromGLBuffer` and its siblings are already generated
(`tools/codegen/profiles/opencl.toml`, §8), so the wire is not what is missing.
What is missing is on the host. The spec is explicit: those calls answer
`CL_INVALID_CONTEXT` unless the `cl_context` "was created from a GL context" -
that is, with `CL_GL_CONTEXT_KHR` in its properties naming a live host GL
context. The device creates its host CL context inside `create_with_options`
from the properties the guest sent, and the guest's GL context is a number only
the guest's OpenGL ICD knows.

So three things would have to exist, none of which is a table:

1. **A way to name a GL context on the wire.** The executor's
   `create_with_options` takes `RvkExecutor *share`, and its contract is "an
   executor of the same sub-API" - a CL context sharing a GL context is a
   different relation and needs a different parameter.
2. **A backend that uses it.** `OpenClBackend` would create its context with
   `CL_GL_CONTEXT_KHR` (and the EGL display the GL backend holds) rather than
   with the plain device, which is a change to how a CL executor is made rather
   than to what it executes.
3. **One object identity across the two ICDs in one process.** The guest's GL
   context lives in `rvkgl.dll` and its CL context in `rvkcl.dll`, each with
   its own `IcdConnection` and its own rvk context. An application handing
   `clCreateFromGLBuffer` a GL name means a name in the *other* DLL's context.
   This is the same problem 055's T033 names - one transport link per process -
   and it is not solvable inside either ICD.

Until those land, `cl_khr_gl_sharing` is not reported and no acquire window is
kept, because there is nothing to acquire: an application that calls
`clCreateFromGLBuffer` reaches the generated refusal, which is the honest
answer. The interop matrix's GL↔CL leg (SC-001) therefore stays open alongside
the OpenGL rows §12 left open, and both are recorded in §10's list of what this
environment cannot prove.

**What did land for OpenCL** is the other half of US2: `ClExternalMemory` and
`clCreateBufferWithProperties`, which take the Win32 handle path all the way to
`OPEN_SHARED_OBJECT`. That one is blocked only on the device's OpenCL import
(T019) - a host away, not a design away.

## §19 — One branch this machine cannot reach, and what the mark is for

T045 asked for 100% branch coverage with the `AV_UNTESTABLE` ratchet at 0. Six
branches stood in the way and five of them were wrong to have: three rollbacks
in the Vulkan ICD that their own `static_assert`s prove can never run (a
publish's and an import's `ExternalObjects::remember`, and `vkCreateDevice`
asking the host for its dma-buf extension), and two refusals in the OpenGL
backend that a test simply had not asked for. The rollbacks are gone - a branch
nothing can take is one no test can judge, and the assertion is what keeps the
assumption true - and the OpenGL ones are now driven, one by importing a native
fence and one by importing a semaphore with no context to make it on.

The sixth is `VulkanBackend::import_object`'s last refusal: the host driver
declining an import the backend asked for correctly. Four ways were tried to
make a driver refuse one, and this machine's only driver that takes a dma-buf
at all - radv - took every one of them:

| what was handed over | what radv did |
|---|---|
| a pipe, as a dma-buf | answered a non-empty memory-type mask |
| a descriptor number that names nothing | answered a non-empty mask |
| a dma-buf of one page, imported as 16 MiB | allocated it |
| the same, on a device that never enabled `VK_EXT_external_memory_dma_buf` | allocated it |

NVIDIA's driver is the other GPU here and takes no dma-buf at all, so it never
reaches the line. The branch is real - a driver that refuses must not be
answered with a handle it never made - and no test on this host can reach it.

So it carries the feature's first `AV_UNTESTABLE`, and the gate learned what
the mark is for. `tools/check_coverage.py` counted marks and ratcheted them but
still held every branch to 100%, which made a mark something nobody could ever
write: the percentage failed whatever it said. It now leaves a marked line out
of the count and lists it in the report, which is what constitution IV.1
describes - "a branch that genuinely cannot be reached from a test is marked
`AV_UNTESTABLE("reason")` at the site. The coverage gate counts those marks,
fails if the count rises without a spec change, and every mark is reviewed."
The baseline moves 0 → 1 with this feature, and the mark names the reason and
points here.


## §20 — Answered: option 1, and what an extension row costs

§12 left T037 two ways out and called the first the smaller. It was built, and
this is what it took - written down because the cost is not where §12 guessed.

**The parameters were the easy half.** `GL_DEDICATED_MEMORY_OBJECT_EXT` and
`GL_PROTECTED_MEMORY_OBJECT_EXT` are set before an import that this design
performs on the device, so there is nothing on the host to set them on. They
are now recorded on the ICD's own row and the *import* refuses with
`GL_INVALID_OPERATION` when either was asked for. That keeps the promise the
row makes: an application that wants dedicated or protected memory is told no,
in the extension's own vocabulary, rather than handed memory that is neither.

**The generator was the larger half, and it found a bug on the way.** A row
promises every command in its extension, and `GL_EXT_memory_object` has 19 of
which 053 answered 4. Ten more are storage commands the guest really does send,
so they are generated now; the two `glGetUnsignedByte*EXT` queries are
generated with them. That is where §12's warning came due:
`glGetUnsignedBytevEXT.data` is `COMPSIZE(pname)`, which the rules *can* read
and read as one value - so a 16-byte device UUID would have been decoded as one
byte and the driver's answer written fifteen bytes past what the decode arena
handed out. Four `[pname_counts]` lines fix it, and the emitted table now says
`case 0x9597: return 16`. The generator refuses only what it cannot read, so
nothing would have caught this but reading the registry.

**And the reporting rule had to learn what a driver-implemented command is.**
Six entry points never cross - `glIsMemoryObjectEXT` and `glIsSemaphoreEXT`
answer from the ICD's table, because a created name is an object before the
host has heard of it, and the two parameter pairs are the recording above. Two
more, the `glImport*Win32NameEXT` pair, are answered in order to refuse: a
named section is what another process opens *by name*, and this path shares by
handle. All twelve are declared `driver_implemented` in the profile, which is
what makes the generator carry the extension - "an application gets a working
entry point for every one of them" - and they are defined as `Wrap_*` beside
the other driver-implemented entries rather than in a second table of the
ICD's own.

**One gate was wrong and is now right.** `tools/check_extension_policy.py`
demanded that the host extension a row is answered over be one the *generator*
implemented, reasoning that "the ICD cannot translate to something this build
never generated". True for Vulkan, where the ICD itself translates
`vkGetMemoryWin32HandleKHR` into `vkGetMemoryFdKHR`. Not true here: the guest
never sends an fd command at all, because the memory object is made inside
`OPEN_SHARED_OBJECT` by `src/device/shim/gl_host_import.cpp`, which resolves
`glImportMemoryFdEXT` itself and checks `GL_EXT_memory_object_fd` on the host.
The gate now accepts a host extension the *device's shim* names, which is still
a check - a row answered over something nothing uses fails exactly as before.

So the four rows are reported when the host lists `GL_EXT_memory_object_fd` and
`GL_EXT_semaphore_fd`, which is the condition `contracts/wire_additions.md`
always stated, and the conformance-subset advice above is withdrawn.
## §21 — The OpenCL leg in the guest: what was proven, and the one thing QEMU refuses

Two things were learnt building the guest's half of the OpenCL matrix, and one
of them changed the design.

**Shared memory cannot be mapped, so the guest cannot write the bytes.** The
probe's first shape filled the exportable allocation with `vkMapMemory`. That
answers `VK_ERROR_MEMORY_MAP_FAILED` for every shared allocation and always
will: `IcdRoot::allocate_memory` sends an allocation that shares memory down
`shared_allocation`, where the memory is the host's whole allocation rather
than a chunk of this process's RAM (feature 047, US3), and `DeviceMemory::map`
answers only for a chunk it placed. So the bytes a second API is judged against
have to be written by the GPU: the probe now makes a `VK_BUFFER_USAGE_TRANSFER_DST_BIT`
buffer *before* the allocation - its `memoryTypeBits` are what the allocation's
type must come from - binds it to the shared memory, and fills it with
`vkCmdFillBuffer` on the probe's own queue. `shared.target`, `shared.exportable`
and `shared.filled` are those three steps, and all three pass in the guest.

**On this host, inside QEMU, rusticl will not make a GL-sharing context.** The
device's OpenCL backend reaches a shared object through `cl_khr_gl_sharing`
(§18): the guest's `cl_context` is created naming a GL context of the device's
own, and `clCreateFromGLBuffer` then names the imported dma-buf. That works.
`tests/unit/device/executor/test_opencl_backend_device.cpp` does the whole
chain against the real host - Vulkan exports a dma-buf, the backend imports it,
and the bytes read back through the `cl_mem` are the bytes Vulkan wrote, in
both directions - and it passes on this machine.

The same code inside the QEMU process fails. `clCreateContext` with
`CL_GL_CONTEXT_KHR` and `CL_EGL_DISPLAY_KHR` answers
`CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR` (-1000) there, every time, and
`clCreateFromGLBuffer` on the ordinary context that follows answers
`CL_INVALID_CONTEXT`. What was measured, in the QEMU process, to find out why:

| Asked | Answer |
|---|---|
| the GL renderer of the vehicle context, and the CL device's name | both `AMD Radeon RX 9070 XT (radeonsi, gfx1201, ACO)` - one GPU, one driver |
| `eglGLInteropQueryDeviceInfoMESA(display, context, ...)`, which is what rusticl asks | `MESA_GLINTEROP_SUCCESS`, the same values as in a passing test process |
| the vehicle context current on the calling thread, and not current | refused either way |
| a GL context created on that thread rather than at bind time | refused |
| every EGL device on the host, each with its own display and context | refused, on all three |
| the property list without `CL_EGL_DISPLAY_KHR` | refused |
| `dlsym(RTLD_DEFAULT, "eglGetProcAddress")`, and the executor loaded `RTLD_GLOBAL` | resolved in both processes; no change |
| the device the guest named against the bound one | the same pointer, the same device |
| `CL_DEVICE_EXTENSIONS` of that device | lists `cl_khr_gl_sharing` |

So it is neither our display, nor our context, nor the thread, nor symbol
visibility, nor the device: the identical calls succeed in an ordinary process
on the same machine and fail inside QEMU's. The reason is somewhere in rusticl
or in Mesa's EGL, and is not answered here.

**What the code does about it.** Nothing claims an ability it has not
exercised. `OpenClBackend::initialize` now *measures*: it creates a GL-sharing
context on the bound device and releases it, and a host that refuses leaves the
backend with no vehicle at all - the same state as a GPU with no EGL display.
`OpenClBackend::on_clGetDeviceInfo` then strikes `cl_khr_gl_sharing` out of the
device's extension string, because that name is what the guest's ICD is to read
as "a shared object can be opened into OpenCL here"; every other query is
forwarded untouched. The guest's probe asks its own driver for
`cl_khr_external_memory_win32` before it names a handle, and writes
`note shared.cl_external_memory` and stops when it is not reported - a note,
not a verdict, because a driver that reports no extension is refusing nothing.

**What is left.** The ICD's row for `cl_khr_external_memory_win32`, answered
over the host's `cl_khr_gl_sharing`. It needs two things this feature did not
build: `ClExtensionPolicy::report` must honour `Row::host_` as the Vulkan and
OpenGL policies do, and `tools/check_extension_policy.py` needs a generated
list of the OpenCL extensions this build carries - `IMPLEMENTED_LISTS` has an
entry per API and OpenCL has none, so any row at all fails the gate today. The
row would be inert on this machine anyway, since the device strikes the host
name out inside QEMU; it lands with the host that can share, or with the answer
to why this one cannot.
