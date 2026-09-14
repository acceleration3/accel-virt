---

description: "Tasks: the guest transport"
---

# Tasks: the guest transport

**Input**: Design documents from `/specs/043-guest-kmt-transport/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/client_transport.md, quickstart.md

**Tests**: Required, and written first. Constitution IV.

**Organization**: by user story, in priority order. Each `**Checkpoint**` is a commit once `ctest --test-dir build -E tools_tests` passes there.

## Format: `[ID] [P?] [Story] Description`

- **[Story]**: US1 (the encoders reach the host), US2 (a blob arena of host-visible memory), US3 (faults reach the client)

---

## Phase 1: Setup - the record that crosses the seam

**Purpose**: the one piece both sides share, before either side is built.

- [X] T001 [P] Write `tests/unit/proto/context/test_rvk_context_record.cpp` first: a well-formed record round-trips; a bad magic, an unknown sub-API, an order outside 12..24, and a reply id colliding with the ring or fence id are each refused - modelled on `test_context_setup.cpp`
- [X] T002 `RvkContextRecord` (object class, shared) in `src/proto/context/rvk_context_record.{hpp,cpp}`: the 32-byte private-data record a process puts on `D3DKMTCreateContextVirtual` - magic `RVKT` 0x544B5652, major/minor, sub_api (exactly one bit), ring_order (12..24), and the three allocation indices for ring, fence and reply (reply 0 means none, otherwise distinct from ring and fence). `decode` validates every field as hostile input; `encode` for the user side. No doorbell field: the miniport owns the doorbell (research §5)

**Checkpoint**: the record encodes and decodes, and refuses a bad one. Commit.

---

## Phase 2: Foundational - the transport interface both ends share

**Purpose**: the interface the generated client is handed and the loopback rig substitutes, before either implementation.

- [X] T003 `ClientTransport` (object interface) in `src/proto/client_transport.hpp`: `request_writer()`, `send_async(id, writer)`, `send_sync(id, writer, reply)`, `decode_arena()`, and `blobs()` - the members the generated client calls (contracts/client_transport.md, section A). `TransportResult` with `Ok`. Protected non-virtual destructor. This is the header `src/proto/generated/*/dispatch_client.h` includes, so the include name it carries changes to this in the emitter (T017)
- [X] T004 [P] Write `tests/harness/transport/mock_transport.{hpp,cpp}` - the loopback-side `ClientTransport` a test drives, recording what was sent and answering scripted replies, so US1's client tests need no guest

**Checkpoint**: the interface compiles and a mock implements it. Commit.

---

## Phase 3: US1 - The generated encoders reach the host (Priority: P1) 🎯 MVP

**Goal**: a synchronous command encoded by the generated client reaches the device over the ring and its answer comes back, with the wait a real sleep.

**Independent test**: the loopback rig creates a context from a record, a client sends `vkCreateInstance` through `KmtTransport`, and the reply decodes - the round trip `test_loopback_context.cpp` already proves for a hand-written frame, now driven through the generated client.

### The miniport learns to serve an rvk context

- [X] T005 [US1] Write `tests/unit/core/scheduling/test_context_table.cpp` additions first: a `CreateContext` carrying a valid `RvkContextRecord` builds an rvk binding; one carrying a bad record is refused with the record's own error, not silently ignored; one with no private data stays a plain scheduling context as today
- [X] T006 [US1] `ContextTable::create` accepts private data of `RvkContextRecord::encoded_size` and builds an `RvkContextBinding` beside the scheduling `Context` (`src/core/scheduling/context_table.cpp`, whose line 21 refuses all private data today)
- [X] T007 [US1] Write `tests/unit/core/rvk/test_context_blob_publisher.cpp` first: the three named allocations become guest blobs the device can name; an allocation not resident, or not the whole blob, is refused; teardown unpins them
- [X] T008 [US1] `ContextBlobPublisher` (system class) in `src/core/rvk/context_blob_publisher.{hpp,cpp}`: turns a context's ring, fence and reply aperture allocations into `RESOURCE_CREATE_BLOB` GUEST resources over the pages VidMm handed the driver (`Allocation::map_aperture`), so RVKC can name them by id. The allocations are `AccessedPhysically` in the aperture segment (research §3). Superseded in T013: the class is deleted; `ContextRequestSequence` publishes the multi-entry blobs over a context's `ContextPages`, under ids `RvkContextBinding` assigns
- [X] T009 [US1] Write `tests/unit/core/rvk/test_rvk_context_binding.cpp` first: a binding drives `ContextRequestSequence` to CTX_CREATE, three blobs and setup; a setup the device refuses faults the binding; teardown reverses it
- [X] T010 [US1] `RvkContextBinding` (object class, KMD) in `src/core/rvk/rvk_context_binding.{hpp,cpp}`: owns the `GuestContext` a dxgkrnl context maps to, and drives its setup and teardown through the publisher's blob ids. PASSIVE for setup, DISPATCH for service (`GuestContext`'s existing annotations)

### Submission and the wait

- [X] T011 [US1] Write `tests/unit/core/rvk/test_submission_reader.cpp` first: a DMA buffer holding a valid RVKS is read into begin/end/fence; a short buffer, a bad magic, a fence not greater than the last, and a range outside the ring are each refused rather than acted on
- [X] T012 [US1] `SubmissionReader` (object class, KMD) in `src/core/rvk/submission_reader.{hpp,cpp}`: copies the guest DMA buffer through its `Platform` once and decodes the `SubmissionDescriptor`, so `Adapter::submit_command_virtual` acts on the batch a process left there instead of ignoring it (`adapter.cpp:1149`, which reads nothing today)
- [X] T013 [US1] `Adapter::submit_command_virtual` reads the DMA buffer through `SubmissionReader`, has the binding's `RingSubmitter` store the tail and ring the doorbell, and no longer completes the fence from its own DPC. The adapter maps BAR 0 doorbells (research §5); the guest submits, the driver rings. *As built:* the RVKS travels as the submission's private data, not in the DMA buffer - `DmaBufferVirtualAddress` is a GPU address the driver cannot read, and dxgkrnl copies private data into kernel memory (`DXGCONTEXT::SubmitCommand`, dxgkrnl.sys @1402d7720); `RvkContextTable` binds a context to the process's own aperture allocations at CreateContext and the node holds each packet until the fence page says the host finished it
- [X] T014 [US1] Write `tests/unit/core/rvk/test_fence_interrupt_route.cpp` first: the vector for context N wakes context N and no other; a vector for a context that has gone is a no-op
- [X] T015 [US1] *Replaced in T013 by `RvkContextTable::serve` and `VirtioGpuDevice::connect_fence_queue`: the fence queues share the granted messages, so the DPC serves every live context.* Originally: `FenceInterruptRoute` (system class, KMD) in `src/core/rvk/fence_interrupt_route.{hpp,cpp}`: gives each context's fence queue (`2+n`, the wire's reserved range, up to `RingSubmitter::doorbell_pages` = 64) an MSI-X vector, and on that interrupt serves the binding whose fence page the device wrote - completing the submission fence dxgkrnl already tracks when the page says the batch finished, so a CPU wait wakes without polling

### The transport a process holds

- [X] T016 [US1] Extend the `Kmt` seam (`src/guest/kmt/kmt.hpp` and `src/um/gdi_kmt.cpp`, with `MockKmt`): create-context carrying private data, a lock that returns a mapped view, and a CPU wait on a context's fence. These are the D3DKMT calls the transport makes and the suite never did (data-model, disagreement 1)
- [X] T017 [US1] Write `tests/unit/guest/transport/test_kmt_transport.cpp` and `test_transport_ring.cpp` and `test_reply_reader.cpp` and `test_transport_fence.cpp` first, over `MockKmt`: a writer is lent over the ring; an overflowed writer is refused by `send_*` rather than sent (contract B1); `head <= tail <= head + 2^order` holds after every publish; a reply is read only from a slot a device wrote, and one answer is valid only until the next `send_sync` (contract B2); the fence says which submission completed
- [X] T018 [US1] `TransportRing`, `ReplyReader`, `TransportFence` (object classes) in `src/guest/transport/`: the ring the writer is lent over and the tail it publishes; the reply reader that refuses an unwritten slot; the fence that maps a ring extent back to the submission that will complete it (max 64 outstanding, data-model)
- [X] T019 [US1] `KmtTransport` (object class) and `GuestConnection` (system class) in `src/guest/transport/`: `KmtTransport` is one context's `ClientTransport` over the `Kmt`, holding the mapped ring, reply and fence; `GuestConnection` holds the one adapter and device a process shares across its per-sub-API transports (US1.1's shared link, data-model disagreement 2)
- [X] T020 [US1] The emitter lends the writer: `client_dispatch.py`, `opencl/emit.py`, `opengl/emit.py` emit `t.request_writer()`, `send_*(id, writer, ...)` and a `WireReader` over the reply, include `proto/client_transport.hpp`, and `send_*` carries the overflow check (research §4). `send_*` refuses an overflowed writer; the `WireArena` sites take a refusing `{nullptr,0}` arena; the two emitter bugs (the `VkResult`-into-unsigned return, the OpenGL read-back lease leak) are fixed. Regenerate; `generated_sources` and `check_generated` stay green
- [X] T021 [US1] The Vulkan and OpenCL generated clients compile and link against `ClientTransport`; a unit test drives `vkCreateInstance` and `clGetPlatformIDs` through the generated client onto the mock transport and reads the reply. OpenGL is left compiling against the contract, not linking, until 050's helpers exist (research §4)
- [X] T022 [US1] The L4 loopback rig gains a `Kmt`-driven path: `LoopbackKmt` over the rig's device, a context created from a record, a batch sent through the generated Vulkan client, the fence waited, the reply decoded - SC-001's round trip

**Checkpoint**: a generated command reaches the device and its answer comes back, the wait a real sleep. MVP; the feature could stop here. Commit.

---

## Phase 4: US2 - A blob arena of host-visible memory (Priority: P1)

**Goal**: a large array crosses as a blob rather than inline, from memory the process mapped.

- [ ] T023 [US2] Write `tests/unit/guest/transport/test_kmt_blob_arena.cpp` first: a lease is handed out only when the arena is ready; a lease carries a writable `data` and an `offset`; `give_back` and `retire_after_last` return room; a request past the arena is not-ready rather than a lease
- [ ] T024 [US2] `KmtBlobArena` (object class) in `src/guest/transport/`, the `ClientBlobArena` the OpenGL client names: `Lease{}` with `ok()`, `data`, `offset`; `for_transport` as a member not a static (principle III); `take`, `give_back`, `retire_after_last`, `ready`. Backed by an aperture allocation the process locked, the same kind the ring uses
- [ ] T025 [US2] *Waiting for 041.* The arena is guest memory the executor imports zero-copy (capset bit 2, proven in 042), not host-visible BAR memory: 041 could not place an allocation in the CPU-visible segment, and that is not this feature's to solve. US2.1's "an allocation from 041 mapped into the process" is the aperture allocation; US2.2's below-4-GiB view is dxgkrnl's to place (041 T023). Record what is unmet and why, in the spec
- [ ] T026 [US2] A retired lease's room comes back when its fence completes: `TransportFence` releasing a lease keyed to the submission that used it, so a long-running client does not exhaust the arena (US2.3)

**Checkpoint**: a large array crosses as a blob, and retired room returns. Commit.

---

## Phase 5: US3 - Faults, TDR and device loss reach the client (Priority: P1)

**Goal**: a faulted context, a TDR and a lost device each reach the waiting client rather than hanging it.

- [X] T027 [US3] Write the fault tests first, over `MockKmt` and the loopback rig: a context the device faults makes the next `send_sync` return not-`Ok` and every one after; a fence page marked faulted becomes `DXGK_INTERRUPT_DMA_FAULTED` for that context and no other; a wait on a faulted context wakes rather than sleeping forever
- [X] T028 [US3] The binding maps a faulted fence page to `ContextLoss`'s `DMA_FAULTED` for the owning context (`adapter.cpp` already has the path for a changed device), and `KmtTransport` turns a faulted wait into a failed call - the client sees loss, not a hang
- [X] T029 [US3] A TDR on the adapter and a device loss reach a waiting client: the wait returns, `send_sync` answers not-`Ok`, and recovery is `CTX_DESTROY` and a fresh setup (research §5). Tested over the loopback rig's reset path

**Checkpoint**: every failure reaches the client. Commit.

---

## Phase 6: Hostile input and the fuzz target

- [X] T030 `tests/fuzz/fuzz_reply_reader.cpp` with a committed corpus: a hostile reply slot and a hostile fence page into `ReplyReader` and `FenceObserver`, the required outcome a refusal or a fault, never a read past the mapping or a crash (SC-002, constitution V)
- [X] T031 The transport core is 100% branch covered, invariants asserted after every mutation, and a hostile-mapping table drives every refusal (SC-002); `coverage_gate` green

**Checkpoint**: the transport distrusts its mappings the way the device distrusts the guest's. Commit.

---

## Phase 7: What the feature promised

- [ ] T032 A transport round trip runs in a real guest: a new scenario in `src/guest/suite/` sends a batch and reads a reply through the transport, and `tools/guest/run_guest_suite.py` reports it passing. The live-executor version of the same path is feature 044 (SC-004)
- [X] T033 `docs/protocol.md` and `docs/kb/11-implementation-plan.md` describe the private-data record and the submission path; the spec's Assumptions section is corrected where 024 and 041 did not provide what it named
- [ ] T034 `AV_GUEST_IMAGE=... tools/gate.sh` passes; merge to `master`; push

---

## Dependencies & Execution Order

- **Phase 1** blocks everything: both sides share the record.
- **Phase 2** blocks every story: the interface both ends implement.
- **US1 (Phase 3)** is the MVP and can stop there - a generated command reaching the host and back is the whole point.
- **US2 (Phase 4)** needs US1's transport to lend leases over.
- **US3 (Phase 5)** needs US1's binding and wait to fault.
- **Phase 6** hardens what Phases 3-5 built.
- **Phase 7** is the guest proof and the merge.

### Parallel opportunities

- T001 and T004 are test files against types that do not exist yet.
- The miniport half (T005-T015) and the transport half (T016-T019) meet only at the record and the wire, so they can be built by two builders once Phase 2 is in - one in `src/core`, one in `src/guest`.
- T023 and T030 are independent test files.

---

## Implementation Strategy

**MVP**: Phases 1-3 - the record, the interface, and a generated command reaching the host and back over a real sleep. **Increment 2**: Phases 4 and 5, the blob arena and the fault paths. **Increment 3**: Phases 6 and 7, hardening and the guest proof.

---

## Notes

- The transport core is portable and builds where `GuestContext` builds; the generated client is not in the no-CRT suite and does not have to be (research §6). Its build home is the ICDs, 045 on.
- Everything read from a mapped ring, reply or fence page is hostile and validated before use, the same rule the device keeps against the guest.
- Nothing on the submission path allocates; the ring, reply and fence are fixed allocations the process locked once.
