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
| two guest *processes* see the same pixels | L5 | no: no prepared guest image here |
| OpenGL imports a descriptor another API exported | L1 on a host GPU | yes: this host's GL lists both fd extensions |
| OpenCL imports a descriptor another API exported | L1 on a host GPU | no: this host's OpenCL is rusticl, which has no `cl_khr_external_memory_dma_buf` |

SC-001 and SC-003 are L5 claims and are not demonstrated by this change; the
gate reports the guest as skipped, and the spec's US1 in-guest half stays open
until an image is available.

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
